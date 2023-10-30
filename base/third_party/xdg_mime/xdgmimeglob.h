/* -*- mode: C; c-file-style: "gnu" -*- */
/* xdgmimeglob.h: Private file.  Datastructure for storing the globs.
 *
 * More info can be found at http://www.freedesktop.org/standards/
 *
 * Copyright (C) 2003  Red Hat, Inc.
 * Copyright (C) 2003  Jonathan Blandford <jrb@alum.mit.edu>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later or AFL-2.0
 */

#ifndef __XDG_MIME_GLOB_H__
#define __XDG_MIME_GLOB_H__

#include "xdgmime.h"

typedef struct XdgGlobHash XdgGlobHash;

typedef enum
{
  XDG_GLOB_LITERAL, /* Makefile */
  XDG_GLOB_SIMPLE,  /* *.gif */
  XDG_GLOB_FULL     /* x*.[ch] */
} XdgGlobType;

  
#ifdef XDG_PREFIX
#define _xdg_mime_glob_read_from_file         XDG_RESERVED_ENTRY(glob_read_from_file)
#define _xdg_glob_hash_new                    XDG_RESERVED_ENTRY(hash_new)
#define _xdg_glob_hash_free                   XDG_RESERVED_ENTRY(hash_free)
#define _xdg_glob_hash_lookup_file_name       XDG_RESERVED_ENTRY(hash_lookup_file_name)
#define _xdg_glob_hash_append_glob            XDG_RESERVED_ENTRY(hash_append_glob)
#define _xdg_glob_determine_type              XDG_RESERVED_ENTRY(determine_type)
#define _xdg_glob_hash_dump                   XDG_RESERVED_ENTRY(hash_dump)
#endif

void         _xdg_mime_glob_read_from_file   (XdgGlobHash *glob_hash,
					      const char  *file_name,
					      int          version_two);
XdgGlobHash *_xdg_glob_hash_new              (void);
void         _xdg_glob_hash_free             (XdgGlobHash *glob_hash);
int          _xdg_glob_hash_lookup_file_name (XdgGlobHash *glob_hash,
					      const char  *text,
					      const char  *mime_types[],
					      int          n_mime_types);
void         _xdg_glob_hash_append_glob      (XdgGlobHash *glob_hash,
					      const char  *glob,
					      const char  *mime_type,
					      int          weight,
					      int          case_sensitive);
XdgGlobType  _xdg_glob_determine_type        (const char  *glob);
void         _xdg_glob_hash_dump             (XdgGlobHash *glob_hash);

#endif /* __XDG_MIME_GLOB_H__ */
