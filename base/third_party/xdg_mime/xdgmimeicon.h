/* -*- mode: C; c-file-style: "gnu" -*- */
/* xdgmimeicon.h: Private file.  Datastructure for storing the aliases.
 *
 * More info can be found at http://www.freedesktop.org/standards/
 *
 * Copyright (C) 2008  Red Hat, Inc.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later or AFL-2.0
 */

#ifndef __XDG_MIME_ICON_H__
#define __XDG_MIME_ICON_H__

#include "xdgmime.h"

typedef struct XdgIconList XdgIconList;

#ifdef XDG_PREFIX
#define _xdg_mime_icon_read_from_file        XDG_ENTRY(icon_read_from_file)
#define _xdg_mime_icon_list_new              XDG_ENTRY(icon_list_new)
#define _xdg_mime_icon_list_free             XDG_ENTRY(icon_list_free)
#define _xdg_mime_icon_list_lookup           XDG_ENTRY(icon_list_lookup)
#define _xdg_mime_icon_list_dump             XDG_ENTRY(icon_list_dump)
#endif

void          _xdg_mime_icon_read_from_file (XdgIconList *list,
					    const char   *file_name);
XdgIconList  *_xdg_mime_icon_list_new       (void);
void          _xdg_mime_icon_list_free      (XdgIconList *list);
const char   *_xdg_mime_icon_list_lookup    (XdgIconList *list,
					     const char  *mime);
void          _xdg_mime_icon_list_dump      (XdgIconList *list);

#endif /* __XDG_MIME_ICON_H__ */
