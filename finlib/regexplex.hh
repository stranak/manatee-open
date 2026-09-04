// Copyright (c) 1999-2023  Pavel Rychly, Milos Jakubicek

#ifndef REGEXPLEX_HH
#define REGEXPLEX_HH

#include "fsop.hh"
#include "regpref.hh"
#include "generator.hh"
#include "lexicon.hh"
#include <config.hh>
#include <memory>

#if defined DARWIN
#include <cctype>
#include <cstring>
// glibc extension, unavailable in Darwin's libc
static inline int strverscmp(const char *s1, const char *s2)
{
    const unsigned char *p1 = (const unsigned char *) s1;
    const unsigned char *p2 = (const unsigned char *) s2;
    while (*p1 && *p2) {
        if (isdigit(*p1) && isdigit(*p2)) {
            const unsigned char *b1 = p1, *b2 = p2;
            while (*p1 == '0') p1++;
            while (*p2 == '0') p2++;
            const unsigned char *d1 = p1, *d2 = p2;
            while (isdigit(*p1)) p1++;
            while (isdigit(*p2)) p2++;
            size_t len1 = p1 - d1, len2 = p2 - d2;
            if (len1 != len2)
                return len1 < len2 ? -1 : 1;
            int cmp = memcmp(d1, d2, len1);
            if (cmp)
                return cmp;
            size_t lead1 = d1 - b1, lead2 = d2 - b2;
            if (lead1 != lead2)
                return lead1 > lead2 ? -1 : 1;
            continue;
        }
        if (*p1 != *p2)
            return (int) *p1 - (int) *p2;
        p1++;
        p2++;
    }
    return (int) *p1 - (int) *p2;
}
#endif

template <class Revidx>
FastStream *regexp2poss (Revidx &rev, lexicon *lex, const char *pattern,
                         const char *locale = NULL, const char *encoding = NULL, 
                         bool ignorecase = false, FastStream* filter_fs = NULL)
{
    unique_ptr<FastStream> filter_fs_ptr(filter_fs);
    regexp_pattern pat (pattern, locale, encoding, ignorecase);

    if (pat.any())
        return new SequenceStream (0, rev.maxpos() - 1, rev.maxpos());

    if (pat.no_meta_chars()) {
        // pat.prefix has been de-escaped
        int id = lex->str2id (pat.prefix());
        if (id < 0)
            return new EmptyStream();
        else
            return rev.id2poss (id);
    }

    vector<string> *parts = pat.disjoint_parts();
    if (parts->size() && !ignorecase) {
        // pattern has form (word1|word2|word3|...)
        vector<FastStream*> *fsv = new std::vector<FastStream*>;
        for (size_t i = 0; i < parts->size(); i++) {
            int id = lex->str2id ((*parts)[i].c_str());
            if (id >= 0)
                fsv->push_back (rev.id2poss (id));
        }
        return QOrVNode::create (fsv);
    }

    if (pat.compile())
        return new EmptyStream();

    filter_fs_ptr.release();
    if (!filter_fs) {
        Generator<int> *ids = lex->pref2ids (pat.prefix());
        if (ids->end()) {
            delete ids;
            return new EmptyStream();
        }
        filter_fs = new Gen2Fast<int> (ids);
    }

    std::vector<FastStream*> *fsv = new std::vector<FastStream*>;
    fsv->reserve(32);
    Position id, finval = filter_fs->final();
    while ((id = filter_fs->next()) < finval) {
        if (pat.match (lex->id2str (id)))
            fsv->push_back(rev.id2poss (id));
    }
    delete filter_fs;
    return QOrVNode::create (fsv);
}

template <class Revidx>
FastStream *compare2poss (Revidx &rev, lexicon *lex, const char *pattern,
                         int order, bool ignorecase = false)
{
    std::vector<FastStream*> *fsv = new std::vector<FastStream*>;
    fsv->reserve(32);
    for (int id = 0; id < lex->size(); id++) {
        const char *str = lex->id2str (id);
        int cmp = strverscmp(str, pattern);
        if ((order < 0 && cmp <= 0) || (order > 0 && cmp >= 0)) {
            FastStream *s = rev.id2poss (id);
            fsv->push_back(s);
        }
    }
    return QOrVNode::create (fsv);
}

Generator<int> *regexp2ids (lexicon *lex, const char *pattern,
        const char *locale = NULL, const char *encoding = NULL,
        bool ignorecase = false, const char *filter_pattern = NULL,
        FastStream *filter_fs = NULL);

IdStrGenerator *regexp2strids (lexicon *lex, const char *pattern,
        const char *locale = NULL, const char *encoding = NULL,
        bool ignorecase = false, const char *filter_pattern = NULL, FastStream *filter_fs = NULL);

#endif

// vim: ts=4 sw=4 sta et sts=4 si cindent tw=80:
