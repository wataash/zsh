/*
 * zle_params.c - ZLE special parameters
 *
 * This file is part of zsh, the Z shell.
 *
 * Copyright (c) 1992-1997 Paul Falstad
 * All rights reserved.
 *
 * Permission is hereby granted, without written agreement and without
 * license or royalty fees, to use, copy, modify, and distribute this
 * software and to distribute modified versions of this software for any
 * purpose, provided that the above copyright notice and the following
 * two paragraphs appear in all copies of this software.
 *
 * In no event shall Paul Falstad or the Zsh Development Group be liable
 * to any party for direct, indirect, special, incidental, or consequential
 * damages arising out of the use of this software and its documentation,
 * even if Paul Falstad and the Zsh Development Group have been advised of
 * the possibility of such damage.
 *
 * Paul Falstad and the Zsh Development Group specifically disclaim any
 * warranties, including, but not limited to, the implied warranties of
 * merchantability and fitness for a particular purpose.  The software
 * provided hereunder is on an "as is" basis, and Paul Falstad and the
 * Zsh Development Group have no obligation to provide maintenance,
 * support, updates, enhancements, or modifications.
 *
 */

#include "zle.mdh"

#include "zle_params.pro"

/*
 * ZLE SPECIAL PARAMETERS:
 *
 * These special parameters are created, with a local scope, when
 * running user-defined widget functions.  Reading and writing them
 * reads and writes bits of ZLE state.  The parameters are:
 *
 * BUFFER   (scalar)   entire buffer contents
 * CURSOR   (integer)  cursor position; 0 <= $CURSOR <= $#BUFFER
 * LBUFFER  (scalar)   portion of buffer to the left of the cursor
 * RBUFFER  (scalar)   portion of buffer to the right of the cursor
 */

#define FN(X) ( (void (*) _((void))) (X) )
static struct zleparam {
    char *name;
    int type;
    void (*setfn) _((void));
    void (*getfn) _((void));
    void (*unsetfn) _((Param, int));
    void *data;
} zleparams[] = {
    { "BUFFER",  PM_SCALAR,  FN(set_buffer),  FN(get_buffer),
	zleunsetfn, NULL },
    { "CURSOR",  PM_INTEGER, FN(set_cursor),  FN(get_cursor),
	zleunsetfn, NULL },
    { "LBUFFER", PM_SCALAR,  FN(set_lbuffer), FN(get_lbuffer),
	zleunsetfn, NULL },
    { "RBUFFER", PM_SCALAR,  FN(set_rbuffer), FN(get_rbuffer),
	zleunsetfn, NULL },
    { "WIDGET", PM_SCALAR | PM_READONLY, NULL, FN(get_widget),
        zleunsetfn, NULL },
    { "LASTWIDGET", PM_SCALAR | PM_READONLY, NULL, FN(get_lwidget),
        zleunsetfn, NULL },
    { "keys", PM_ARRAY | PM_READONLY, NULL, FN(get_keys),
        zleunsetfn, NULL },
    { NULL, 0, NULL, NULL, NULL, NULL }
};

/**/
void
makezleparams(int ro)
{
    struct zleparam *zp;

    for(zp = zleparams; zp->name; zp++) {
	Param pm = createparam(zp->name, (zp->type |PM_SPECIAL|PM_REMOVABLE|
					  (ro ? PM_READONLY : 0)));
	if (!pm)
	    pm = (Param) paramtab->getnode(paramtab, zp->name);
	DPUTS(!pm, "param not set in makezleparams");

	pm->level = locallevel;
	pm->u.data = zp->data;
	switch(PM_TYPE(zp->type)) {
	    case PM_SCALAR:
		pm->sets.cfn = (void (*) _((Param, char *))) zp->setfn;
		pm->gets.cfn = (char *(*) _((Param))) zp->getfn;
		break;
	    case PM_ARRAY:
		pm->sets.afn = (void (*) _((Param, char **))) zp->setfn;
		pm->gets.afn = (char **(*) _((Param))) zp->getfn;
		break;
	    case PM_INTEGER:
		pm->sets.ifn = (void (*) _((Param, long))) zp->setfn;
		pm->gets.ifn = (long (*) _((Param))) zp->getfn;
		break;
	}
	pm->unsetfn = zp->unsetfn;
    }
}

/* Special unset function for ZLE special parameters: act like the standard *
 * unset function if this is a user-initiated unset, but nothing is done if *
 * the parameter is merely going out of scope (which it will do).           */

/**/
static void
zleunsetfn(Param pm, int exp)
{
    if(exp)
	stdunsetfn(pm, exp);
}

/**/
static void
set_buffer(Param pm, char *x)
{
    if(x) {
	unmetafy(x, &ll);
	sizeline(ll);
	strcpy((char *)line, x);
	zsfree(x);
	if(cs > ll)
	    cs = ll;
    } else
	cs = ll = 0;
}

/**/
static char *
get_buffer(Param pm)
{
    return metafy((char *)line, ll, META_HEAPDUP);
}

/**/
static void
set_cursor(Param pm, long x)
{
    if(x < 0)
	cs = 0;
    else if(x > ll)
	cs = ll;
    else
	cs = x;
}

/**/
static long
get_cursor(Param pm)
{
    return cs;
}

/**/
static void
set_lbuffer(Param pm, char *x)
{
    char *y;
    int len;

    if(x)
	unmetafy(y = x, &len);
    else
	y = "", len = 0;
    sizeline(ll - cs + len);
    memmove(line + len, line + cs, ll - cs);
    memcpy(line, y, len);
    ll = ll - cs + len;
    cs = len;
    zsfree(x);
}

/**/
static char *
get_lbuffer(Param pm)
{
    return metafy((char *)line, cs, META_HEAPDUP);
}

/**/
static void
set_rbuffer(Param pm, char *x)
{
    char *y;
    int len;

    if(x)
	unmetafy(y = x, &len);
    else
	y = "", len = 0;
    sizeline(ll = cs + len);
    memcpy(line + cs, y, len);
    zsfree(x);
}

/**/
static char *
get_rbuffer(Param pm)
{
    return metafy((char *)line + cs, ll - cs, META_HEAPDUP);
}

/**/
static char *
get_widget(Param pm)
{
    return bindk->nam;
}

/**/
static char *
get_lwidget(Param pm)
{
    return (lbindk ? lbindk->nam : "");
}

/**/
static char **
get_keys(Param pm)
{
    char **r, **q, *p, *k, c;

    r = (char **) halloc((strlen(keybuf) + 1) * sizeof(char *));
    for (q = r, p = keybuf; (c = *p); q++, p++) {
	k = *q = (char *) halloc(5);
	if (c & 0x80) {
	    *k++ = 'M';
	    *k++ = '-';
	    c &= 0x7f;
	}
	if (c < 32 || c == 0x7f) {
	    *k++ = '^';
	    c ^= 64;
	}
	*k++ = c;
	*k = '\0';
    }
    *q = NULL;

    return r;
}
