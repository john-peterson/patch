/* backupfile.c -- make Emacs style backup file names
   Copyright (C) 1990,1991,1992,1993,1995,1997 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING.
   If not, write to the Free Software Foundation,
   59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

/* Written by David MacKenzie <djm@gnu.ai.mit.edu>.
   Some algorithms adapted from GNU Emacs. */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <argmatch.h>
#include <backupfile.h>

#include <stdio.h>
#include <sys/types.h>
#if HAVE_STRING_H
# include <string.h>
#else
# include <strings.h>
#endif

#if HAVE_DIRENT_H
# include <dirent.h>
# define NLENGTH(direct) strlen ((direct)->d_name)
#else
# define dirent direct
# define NLENGTH(direct) ((size_t) (direct)->d_namlen)
# if HAVE_SYS_NDIR_H
#  include <sys/ndir.h>
# endif
# if HAVE_SYS_DIR_H
#  include <sys/dir.h>
# endif
# if HAVE_NDIR_H
#  include <ndir.h>
# endif
#endif

#if CLOSEDIR_VOID
/* Fake a return value. */
# define CLOSEDIR(d) (closedir (d), 0)
#else
# define CLOSEDIR(d) closedir (d)
#endif

#if STDC_HEADERS
# include <stdlib.h>
#else
char *malloc ();
#endif

#if HAVE_DIRENT_H || HAVE_NDIR_H || HAVE_SYS_DIR_H || HAVE_SYS_NDIR_H
# define HAVE_DIR 1
#else
# define HAVE_DIR 0
#endif

#if HAVE_LIMITS_H
# include <limits.h>
#endif
#ifndef CHAR_BIT
#define CHAR_BIT 8
#endif
/* Upper bound on the string length of an integer converted to string.
   302 / 1000 is ceil (log10 (2.0)).  Subtract 1 for the sign bit;
   add 1 for integer division truncation; add 1 more for a minus sign.  */
#define INT_STRLEN_BOUND(t) ((sizeof (t) * CHAR_BIT - 1) * 302 / 1000 + 2)

#define ISDIGIT(c) (((unsigned) (c) - '0') <= 9)

#ifdef _POSIX_VERSION
/* POSIX does not require that the d_ino field be present, and some
   systems do not provide it. */
# define REAL_DIR_ENTRY(dp) 1
#else
# define REAL_DIR_ENTRY(dp) ((dp)->d_ino != 0)
#endif

/* Which type of backup file names are generated. */
enum backup_type backup_type = none;

/* The extension added to file names to produce a simple (as opposed
   to numbered) backup file name. */
char const *simple_backup_suffix = ".orig";

static int max_backup_version __BACKUPFILE_P ((char const *, char const *));
static int version_number __BACKUPFILE_P ((char const *, char const *, size_t));

/* Return the name of the new backup file for file FILE,
   allocated with malloc.  Return 0 if out of memory.
   FILE must not end with a '/' unless it is the root directory.
   Do not call this function if backup_type == none. */

char *
find_backup_file_name (file)
     char const *file;
{
  size_t backup_suffix_size_max;
  char *s;

  /* Allow room for simple or `.~N~' backups.  */
  backup_suffix_size_max = strlen (simple_backup_suffix) + 1;
  if (HAVE_DIR && backup_suffix_size_max < INT_STRLEN_BOUND (int) + 4)
    backup_suffix_size_max = INT_STRLEN_BOUND (int) + 4;

  s = malloc (strlen (file) + backup_suffix_size_max);
  if (s)
    {
      strcpy (s, file);

#if HAVE_DIR
      if (backup_type != simple)
	{
	  int highest_backup;
	  size_t dir_len = base_name (s) - s;

	  strcpy (s + dir_len, ".");
	  highest_backup = max_backup_version (file + dir_len, s);
	  if (! (backup_type == numbered_existing && highest_backup == 0))
	    {
	      sprintf (s, "%s.~%d~", file, highest_backup + 1);
	      return s;
	    }
	  strcpy (s, file);
	}
#endif /* HAVE_DIR */

      addext (s, simple_backup_suffix, '~');
    }
  return s;
}

#if HAVE_DIR

/* Return the number of the highest-numbered backup file for file
   FILE in directory DIR.  If there are no numbered backups
   of FILE in DIR, or an error occurs reading DIR, return 0.  */

static int
max_backup_version (file, dir)
     char const *file, *dir;
{
  DIR *dirp;
  struct dirent *dp;
  int highest_version;
  int this_version;
  size_t file_name_length;

  dirp = opendir (dir);
  if (!dirp)
    return 0;

  highest_version = 0;
  file_name_length = strlen (file);

  while ((dp = readdir (dirp)) != 0)
    {
      if (!REAL_DIR_ENTRY (dp) || NLENGTH (dp) < file_name_length + 4)
	continue;

      this_version = version_number (file, dp->d_name, file_name_length);
      if (this_version > highest_version)
	highest_version = this_version;
    }
  if (CLOSEDIR (dirp) != 0)
    return 0;
  return highest_version;
}

/* If BACKUP is a numbered backup of BASE, return its version number;
   otherwise return 0.  BASE_LENGTH is the length of BASE.  */

static int
version_number (base, backup, base_length)
     char const *base;
     char const *backup;
     size_t base_length;
{
  int version;
  char const *p;

  version = 0;
  if (strncmp (base, backup, base_length) == 0
      && backup[base_length] == '.'
      && backup[base_length + 1] == '~')
    {
      for (p = &backup[base_length + 2]; ISDIGIT (*p); ++p)
	version = version * 10 + *p - '0';
      if (p[0] != '~' || p[1])
	version = 0;
    }
  return version;
}
#endif /* HAVE_DIR */

static char const * const backup_args[] =
{
  "never", "simple", "nil", "existing", "t", "numbered", 0
};

static enum backup_type const backup_types[] =
{
  simple, simple, numbered_existing, numbered_existing, numbered, numbered
};

/* Return the type of backup indicated by VERSION.
   Unique abbreviations are accepted. */

enum backup_type
get_version (version)
     char const *version;
{
  int i;

  if (version == 0 || *version == 0)
    return numbered_existing;
  i = argmatch (version, backup_args);
  if (i < 0)
    {
      invalid_arg ("version control type", version, i);
      exit (2);
    }
  return backup_types[i];
}
