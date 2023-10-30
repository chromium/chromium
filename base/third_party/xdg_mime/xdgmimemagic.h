/* -*- mode: C; c-file-style: "gnu" -*- */
/* xdgmimemagic.h: Private file.  Datastructure for storing the magic files.
 *
 * More info can be found at http://www.freedesktop.org/standards/
 *
 * Copyright (C) 2003  Red Hat, Inc.
 * Copyright (C) 2003  Jonathan Blandford <jrb@alum.mit.edu>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later or AFL-2.0
 */

#ifndef __XDG_MIME_MAGIC_H__
#define __XDG_MIME_MAGIC_H__

#include <unistd.h>
#include "xdgmime.h"
typedef struct XdgMimeMagic XdgMimeMagic;

#ifdef XDG_PREFIX
#define _xdg_mime_glob_read_from_file             XDG_RESERVED_ENTRY(glob_read_from_file)
#define _xdg_mime_magic_new                       XDG_RESERVED_ENTRY(magic_new)
#define _xdg_mime_magic_read_from_file            XDG_RESERVED_ENTRY(magic_read_from_file)
#define _xdg_mime_magic_free                      XDG_RESERVED_ENTRY(magic_free)
#define _xdg_mime_magic_get_buffer_extents        XDG_RESERVED_ENTRY(magic_get_buffer_extents)
#define _xdg_mime_magic_lookup_data               XDG_RESERVED_ENTRY(magic_lookup_data)
#endif


XdgMimeMagic *_xdg_mime_magic_new                (void);
void          _xdg_mime_magic_read_from_file     (XdgMimeMagic *mime_magic,
						  const char   *file_name);
void          _xdg_mime_magic_free               (XdgMimeMagic *mime_magic);
int           _xdg_mime_magic_get_buffer_extents (XdgMimeMagic *mime_magic);
const char   *_xdg_mime_magic_lookup_data        (XdgMimeMagic *mime_magic,
						  const void   *data,
						  size_t        len,
						  int          *result_prio,
						  const char   *mime_types[],
						  int           n_mime_types);

#endif /* __XDG_MIME_MAGIC_H__ */
