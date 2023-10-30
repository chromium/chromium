/* -*- mode: C; c-file-style: "gnu" -*- */
/* xdgmimeicon.c: Private file.  Datastructure for storing the aliases.
 *
 * More info can be found at http://www.freedesktop.org/standards/
 *
 * Copyright (C) 2008  Red Hat, Inc.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later or AFL-2.0
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "xdgmimeicon.h"
#include "xdgmimeint.h"
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <fnmatch.h>

#ifndef	FALSE
#define	FALSE	(0)
#endif

#ifndef	TRUE
#define	TRUE	(!FALSE)
#endif

typedef struct XdgIcon XdgIcon;

struct XdgIcon 
{
  char *mime_type;
  char *icon_name;
};

struct XdgIconList
{
  struct XdgIcon *icons;
  int n_icons;
};

XdgIconList *
_xdg_mime_icon_list_new (void)
{
  XdgIconList *list;

  list = malloc (sizeof (XdgIconList));

  list->icons = NULL;
  list->n_icons = 0;

  return list;
}

void         
_xdg_mime_icon_list_free (XdgIconList *list)
{
  int i;

  if (list->icons)
    {
      for (i = 0; i < list->n_icons; i++)
	{
	  free (list->icons[i].mime_type);
	  free (list->icons[i].icon_name);
	}
      free (list->icons);
    }
  free (list);
}

static int
icon_entry_cmp (const void *v1, const void *v2)
{
  return strcmp (((XdgIcon *)v1)->mime_type, ((XdgIcon *)v2)->mime_type);
}

const char  *
_xdg_mime_icon_list_lookup (XdgIconList *list,
			    const char  *mime_type)
{
  XdgIcon *entry;
  XdgIcon key;

  if (list->n_icons > 0)
    {
      key.mime_type = (char *)mime_type;
      key.icon_name = NULL;

      entry = bsearch (&key, list->icons, list->n_icons,
		       sizeof (XdgIcon), icon_entry_cmp);
      if (entry)
        return entry->icon_name;
    }

  return NULL;
}

void
_xdg_mime_icon_read_from_file (XdgIconList *list,
			       const char   *file_name)
{
  FILE *file;
  char line[255];
  int alloc;

  file = fopen (file_name, "r");

  if (file == NULL)
    return;

  /* FIXME: Not UTF-8 safe.  Doesn't work if lines are greater than 255 chars.
   * Blah */
  alloc = list->n_icons + 16;
  list->icons = realloc (list->icons, alloc * sizeof (XdgIcon));
  while (fgets (line, 255, file) != NULL)
    {
      char *sep;
      if (line[0] == '#')
	continue;

      sep = strchr (line, ':');
      if (sep == NULL)
	continue;
      *(sep++) = '\000';
      sep[strlen (sep) -1] = '\000';
      if (list->n_icons == alloc)
	{
	  alloc <<= 1;
	  list->icons = realloc (list->icons, 
				   alloc * sizeof (XdgIcon));
	}
      list->icons[list->n_icons].mime_type = strdup (line);
      list->icons[list->n_icons].icon_name = strdup (sep);
      list->n_icons++;
    }
  list->icons = realloc (list->icons, 
			   list->n_icons * sizeof (XdgIcon));

  fclose (file);  
  
  if (list->n_icons > 1)
    qsort (list->icons, list->n_icons, 
           sizeof (XdgIcon), icon_entry_cmp);
}


void
_xdg_mime_icon_list_dump (XdgIconList *list)
{
  int i;

  if (list->icons)
    {
      for (i = 0; i < list->n_icons; i++)
	{
	  printf ("%s %s\n", 
		  list->icons[i].mime_type,
		  list->icons[i].icon_name);
	}
    }
}


