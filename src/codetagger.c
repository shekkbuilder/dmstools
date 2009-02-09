/*
 *  DMS Tools and Utilities
 *  Copyright (C) 2008 David M. Syzdek <david@syzdek.net>.
 *
 *  @SYZDEK_LICENSE_HEADER_START@
 *  
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License Version 2 as
 *  published by the Free Software Foundation.
 *  
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *  
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *  
 *  @SYZDEK_LICENSE_HEADER_END@
 */
/**
 *  @file src/codetagger.c Utility for making bulk changes to tqgged text files.
 */
/*
 *  Simple Build:
 *     gcc -W -Wall -O2 -c codetagger.c
 *     gcc -W -Wall -O2 -o codetagger   codetagger.o
 *
 *  GNU Libtool Build:
 *     libtool --mode=compile gcc -W -Wall -g -O2 -c codetagger.c
 *     libtool --mode=link    gcc -W -Wall -g -O2 -o codetagger codetagger.lo
 *
 *  GNU Libtool Install:
 *     libtool --mode=install install -c codetagger /usr/local/bin/codetagger
 *
 *  GNU Libtool Clean:
 *     libtool --mode=clean rm -f codetagger.lo codetagger
 */
#define _DMSTOOLS_SRC_CODINGTAGGER_C 1

/////////////
//         //
//  Notes  //
//         //
/////////////

/*
 *  Debug Levels:
 *      0 - errors only
 *      1 - warnings
 *      2 - file processing notices
 *      3 - tag processing notices
 *      4 - general processing notices
 *      5 - detailed general processing notices
 *      6 - file operations
 *      7 - memory operations
 *      8 - variable dumps
 *      Debug levels are accumulative.
 */

///////////////
//           //
//  Headers  //
//           //
///////////////

#ifdef HAVE_COMMON_H
#include "common.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <getopt.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdarg.h>
#include <regex.h>


///////////////////
//               //
//  i18l Support //
//               //
///////////////////

#ifdef HAVE_GETTEXT
#   include <gettext.h>
#   include <libintl.h>
#   define _(String) gettext (String)
#   define gettext_noop(String) String
#   define N_(String) gettext_noop (String)
#else
#   define _(String) (String)
#   define N_(String) String
#   define textdomain(Domain)
#   define bindtextdomain(Package, Directory)
#endif


///////////////////
//               //
//  Definitions  //
//               //
///////////////////

#ifndef PROGRAM_NAME
#define PROGRAM_NAME "codetagger"
#endif
#ifndef PACKAGE_BUGREPORT
#define PACKAGE_BUGREPORT "david@syzdek.net"
#endif
#ifndef PACKAGE_NAME
#define PACKAGE_NAME ""
#endif
#ifndef PACKAGE_VERSION
#define PACKAGE_VERSION ""
#endif

#define CODETAGGER_OPT_CONTINUE    0x0001
#define CODETAGGER_OPT_DEBUG       0x0002
#define CODETAGGER_OPT_FORCE       0x0004
#define CODETAGGER_OPT_HIDDEN      0x0008
#define CODETAGGER_OPT_LINKS       0x0010
#define CODETAGGER_OPT_QUIET       0x0020
#define CODETAGGER_OPT_RECURSE     0x0040
#define CODETAGGER_OPT_TEST        0x0080
#define CODETAGGER_OPT_VERBOSE     0x0100

#undef  CODETAGGER_STR_LEN
#define CODETAGGER_STR_LEN  512

#ifndef PARAMS
#define PARAMS(protos) protos
#endif


/////////////////
//             //
//  Datatypes  //
//             //
/////////////////

/// tag data
typedef struct tag_data TagData;
struct tag_data
{
   char     * name;
   char    ** contents;
   regex_t    regex;
};


/// config data
typedef struct config_data Config;
struct config_data
{
   unsigned      opts;
   unsigned      tagCount;
   TagData    ** tagList;
   const char  * tagFile;
   const char  * leftTagString;
   const char  * rightTagString;
};


/////////////////
//             //
//  Variables  //
//             //
/////////////////

/*
 *  I normally avoid global variables, however the following
 *  variables significantly simplify managing debug levels and
 *  test modes.  I feel the amount of complexity removed justified
 *  the addition of these variables. --David M. Syzdek 2008/06/21
 */
//int verbosity = 0;	///< sets verbosity of program
//int test_mode = 0;	///< toggles test mode


//////////////////
//              //
//  Prototypes  //
//              //
//////////////////

// prints debug messages
void codetagger_debug PARAMS((int level, const char * fmt, ...));

// escapes each character of the string and places the result at the end of the buffer
int codetagger_escape_string PARAMS((char * buff, const char * str, unsigned len));

// processes original file by inserting/expanding tags
int expand_tags PARAMS((const char * filename, Config * cnf));

// replaces original file with temp file and unlinks temp file name
int codetagger_file_link PARAMS((FILE * fdout, const char * ofile, const char * tfile));

// opens temporary file for writing using mkstemp()
FILE * codetagger_file_open PARAMS((const char * file, char * buff, int buff_len));

// writes formatted data to temp file if the program is not in test mode
void codetagger_file_printf PARAMS((FILE * fdout, const char * fmt, ...));

// reads file into an array
char ** codetagger_get_file_contents PARAMS((const char * file));

// finds a specific tag in the array of tag data
TagData * codetagger_find_tag PARAMS((const char * tagName, TagData ** tagList,
                           const char * fileName, int lineNumber));

// frees memory used to hold file contents
void codetagger_free_filedata PARAMS((char ** lines));

// frees memory used store tag
void codetagger_free_tag PARAMS((TagData * tag));

// frees memory used store an array of tags
void codetagger_free_taglist PARAMS((TagData ** taglist));

// generate array of tags from file
int codetagger_generate_taglist PARAMS((Config * cnf));

// main statement
int main PARAMS((int argc, char * argv[]));

// displays usage
void codetagger_usage PARAMS((void));

// displays version information
void codetagger_version PARAMS((void));

// extracts tag data from tag file
int codetagger_process_tag PARAMS((char * name, char ** data, int len, int pos,
                        Config * cnf));


/////////////////
//             //
//  Functions  //
//             //
/////////////////

/// prints debug messages
/// @param[in] level the verbose level of the current message
/// @param[in] fmt   the format string of the message
/// @param[in] ...   the format arguments of the message
void codetagger_debug(int level, const char * fmt, ...)
{
   va_list arg;

   if (level > 0)
      return;

   va_start (arg, fmt);
      vprintf(fmt, arg);
   va_end (arg);

   return;
}


/// escapes each character of the string and places the result at the end of the buffer
/// @param[in] buff  buffer into which escape the string
/// @param[in] str   string to escape and concatenate from
/// @param[in] len   size of buffer
int codetagger_escape_string(char * buff, const char * str, unsigned len)
{
   unsigned strpos;
   unsigned buffpos;
   
   len--;
   buff[len] = '\0';
   
   buffpos = strlen(buff) + 2;
   for(strpos = 0; ( (strpos < strlen(str)) && (buffpos < len)); strpos++)
   {
      buff[buffpos-2] = '\\';
      buff[buffpos-1] = str[strpos];
      buffpos += 2;
   };
   buff[buffpos] = '\0';
   return(0);
}


/// processes original file by inserting/expanding tags
/// @param[in]  file  name of file to process
/// @param[in]  cnf   array of tags to insert into new file
int expand_tags(const char * filename, Config * cnf)
{
   /* declares local vars */
   int           data_pos;
   int           data_len;
   int           tag_pos;
   int           err;
   unsigned      tagCount;
   TagData     * tag;
   char       ** data;
   char          tmpfile[CODETAGGER_STR_LEN];
   char          margin[CODETAGGER_STR_LEN];
   char          errmsg[CODETAGGER_STR_LEN];
   char          tagName[CODETAGGER_STR_LEN];
   char          regstr[CODETAGGER_STR_LEN];
   char          tagend[CODETAGGER_STR_LEN];
   FILE        * fdout;
   regex_t       regex;
   regmatch_t    match[5];

   /* initialzes data */
   fdout      = NULL;
   tagCount   = 0;
   tmpfile[0] = '\0';
   memset(regstr, 0, CODETAGGER_STR_LEN);

   /* reads file and stores contents */
   if (!(data = codetagger_get_file_contents(filename)))
      return(1);

   /* creates temp file */
   if (!(cnf->opts & CODETAGGER_OPT_TEST))
   {
      if (!(fdout = codetagger_file_open(filename, tmpfile, CODETAGGER_STR_LEN)))
      {
         codetagger_free_filedata(data);
         return(1);
      };
   };

   /* compiles regex used to locate tags */
   codetagger_debug(5, _("   compiling regular expression\n"));
   strncat(regstr,       "(.*)",              CODETAGGER_STR_LEN);
   codetagger_escape_string(regstr, cnf->leftTagString,  CODETAGGER_STR_LEN);
   strncat(regstr,       "([_[:blank:][:alnum:]]+)START",   CODETAGGER_STR_LEN);
   codetagger_escape_string(regstr, cnf->rightTagString, CODETAGGER_STR_LEN);
   if ((err = regcomp(&regex, regstr, REG_EXTENDED|REG_ICASE)))
   {
      regerror(err, &regex, errmsg, CODETAGGER_STR_LEN-1);
      printf("regex error: %s\n", errmsg);
      codetagger_file_link(fdout, NULL, tmpfile);
      codetagger_free_filedata(data);
      return(0);
   };
   codetagger_debug(8, _("   compiled regular expression \"%s\"\n"), regstr);

   /* counts lines in data */
   for(data_len = 0; data[data_len]; data_len++);

   /* loops through file */
   codetagger_debug(5, _("   expanding tags\n"));
   for(data_pos = 0; data_pos < data_len; data_pos++)
   {
      /* copies the old file into the new file */
      codetagger_file_printf(fdout, "%s\n", data[data_pos]);

      /* checks line for a tag */
      if ((err = regexec(&regex, data[data_pos], 5, match, 0)))
         continue;

      /* copies tag information into buffers */
      memset(margin,  0, CODETAGGER_STR_LEN);
      memset(tagName, 0, CODETAGGER_STR_LEN);
      memcpy(margin,  &data[data_pos][(int)match[1].rm_so], match[1].rm_eo - match[1].rm_so);
      memcpy(tagName, &data[data_pos][(int)match[2].rm_so], match[2].rm_eo - match[2].rm_so);

      /* applies the tag to the new file */
      if ((tag = codetagger_find_tag(tagName, cnf->tagList, filename, data_pos+1)))
      {
         /* writes contents of the tag */
         codetagger_debug(3, _("   inserting tag \"%s\" (line %i)\n"), tagName, data_pos);
         for(tag_pos = 0; tag->contents[tag_pos]; tag_pos++)
            codetagger_file_printf(fdout, "%s%s\n", margin, tag->contents[tag_pos]);

         /* searches for end tag */
         err = 1;
         while((data_pos < (data_len-1)) && (err))
         { 
            data_pos++;
            err = regexec(&tag->regex, data[data_pos], 5, match, 0);
         };

         /* exits with error if end tag is not found */
         if (err)
         {
            fprintf(stderr, _("%s: missing end @%sEND@ in %s\n"), PROGRAM_NAME, tagName, filename);
            codetagger_file_link(fdout, NULL, tmpfile);
            codetagger_free_filedata(data);
            return(-1);
         };
         
         /* copies end tag into buffer */
         memset(tagend, 0, CODETAGGER_STR_LEN); 
         memcpy(tagend,  &data[data_pos][(int)match[1].rm_so], match[1].rm_eo - match[1].rm_so);
  
         /* prints end tag */
         codetagger_file_printf(fdout, "%s%s%s%s\n", margin, cnf->leftTagString,
                     tagend, cnf->rightTagString);

         /* counts tags */
         tagCount++;
      };
   };

   /* closes and saves file */
   if (!(tagCount))
   {
      fprintf(stderr, PROGRAM_NAME ": no tags were found in \"%s\"\n", filename);
      codetagger_file_link(fdout, NULL, tmpfile);
   } else {
      codetagger_file_link(fdout, filename, tmpfile);
   };

   /* frees memory */
   codetagger_free_filedata(data);

   /* ends function */
   return(0);
}


/// replaces original file with temp file and unlinks temp file name
/// @param[in] fdout  the file stream of the temp file
/// @param[in] ofile  the original file name
/// @param[in] tfile  the temporary file name    
int codetagger_file_link(FILE * fdout, const char * ofile, const char * tfile)
{
   /* declares local vars */
   struct stat sb;

   /* closes file */
   if (!(fdout))
      return(0);
   fclose(fdout);

   /* exits if temporary file name is not specified */
   if (!(tfile))
      return(0);

   /* assumes clean-up mode if original file is not specified */
   if ( (tfile) && (!(ofile)) )
   {
      unlink(tfile); // removes temparary file
      return(0);
   };

   /* gathers information about original file to copy to temporary file */
   if (stat(ofile, &sb))
   {
      perror(PROGRAM_NAME ": stat()");
      return(-1);
   };

   /* sets file permissions of temporary file */
   if (chmod(tfile, sb.st_mode) == -1)
   {
      perror(PROGRAM_NAME ": chmod()");
      return(-1);
   };

   /* sets file ownerships of temporary file if run as root */
   if (!(geteuid()))
   {
      if (chown(tfile, sb.st_uid, sb.st_gid) == -1)
      {
         perror(PROGRAM_NAME ": chown()");
         return(-1);
      };
   };

   /* removes original file */
   if (unlink(ofile) == -1)
   {
      perror(PROGRAM_NAME ": unlink()");
      return(-1);
   };

   /* links temporary file to original file name */
   if (link(tfile, ofile) == -1)
   {
      perror(PROGRAM_NAME ": link()");
      return(-1);
   };

   /* removes temporary file */
   if (unlink(tfile) == -1)
   {
      perror(PROGRAM_NAME ": unlink()");
      return(-1);
   };

   /* ends function */
   return(0);
}


/// opens temporary file for writing using mkstemp()
/// @params[in] file      file name of file to open for writing
/// @params[in] buff      buffer to store temporary file name
/// @params[in] buff_len  length of buffer
FILE * codetagger_file_open(const char * file, char * buff, int buff_len)
{
   int    fd;
   FILE * fdout;

   buff[0] = '\0';

   //if (test_mode)
   //   return(NULL);

   snprintf(buff, buff_len, "%s.XXXXXXXXXX", file);
   if (!(fd = mkstemp(buff)))
   {
      perror(PROGRAM_NAME ": mkstemp()");
      return(NULL);
   };

   if (!(fdout = fdopen(fd, "w")))
   {
      perror(PROGRAM_NAME ": fdpopen()");
      return(NULL);
   };

   return(fdout);
}


/// writes formatted data to temp file if the program is not in test mode
/// @param[in]  fdout  file stream to write data
/// @param[in]  fmt    format of data to write to stream
/// @param[in]  ...    format arguments
void codetagger_file_printf(FILE * fdout, const char * fmt, ...)
{
   va_list arg;

   //if (test_mode)
   //   return;

   va_start (arg, fmt);
      vfprintf(fdout, fmt, arg);
   va_end (arg);

   return;
}


/// finds a specific tag in the array of tag data
/// @param[in] name name of tag to find
/// @param[in] taglist array of tags to search
/// @param[in] line_number line number of the current file being processed
TagData * codetagger_find_tag(const char * tagName, TagData ** tagList,
                   const char * fileName, int lineNumber)
{
   int i;

   if (!(tagName))
      return(NULL);
   if (!(tagList))
      return(NULL);
   if (!(fileName))
      return(NULL);

   codetagger_debug(5, _("   retrieving tag \"%s\"\n"), tagName);

   for(i = 0; tagList[i]; i++)
      if (!(strcasecmp(tagName, tagList[i]->name)))
         return(tagList[i]);

   codetagger_debug(1, _(PROGRAM_NAME ": %s: %i: unknown tag \"%s\"\n"), fileName, lineNumber, tagName);

   return(NULL);
}


/// frees memory used to hold file contents
/// @param[in]  lines  array of lines to free
void codetagger_free_filedata(char ** lines)
{
   if (!(lines))
      return;

   if (lines[0])
      free(lines[0]);
   lines[0] = NULL;

   free(lines);

   return;
}


/// frees memory used store tag
/// @param[in]  tag  tag data structure to free
void codetagger_free_tag(TagData * tag)
{
   int i;

   if (!(tag))
      return;

   if (tag->name)
   {
      codetagger_debug(7, _("   freeing tag \"%s\"\n"), tag->name);
      free(tag->name);
   } else {
      codetagger_debug(7, _("   freeing unknown tag\n"));
   };

   if (tag->contents)
   {
      regfree(&tag->regex);
      for(i = 0; tag->contents[i]; i++)
         free(tag->contents[i]);
      free(tag->contents);
   };

   free(tag);

   return;
}


/// frees memory used store an array of tags
/// @param[in]  taglist  array of tags to free
void codetagger_free_taglist(TagData ** taglist)
{
   int i;
   if (!(taglist))
      return;
   codetagger_debug(7, _("freeing tag index\n"));
   for(i = 0; taglist[i]; i++)
      codetagger_free_tag(taglist[i]);
   free(taglist);
   return;
}


/// generate array of tags from file
/// @param[in] file  file name of file which contains tag definitions
int codetagger_generate_taglist(Config * cnf)
{
   /* declares local vars */
   int           i;
   int           err;
   int           lineCount;
   char          regstr[CODETAGGER_STR_LEN];
   char          errmsg[CODETAGGER_STR_LEN];
   char          tagName[CODETAGGER_STR_LEN];
   char       ** data;
   regex_t       regex;
   regmatch_t    match[5];

   /* initialize data */
   memset(regstr,   0, CODETAGGER_STR_LEN);
   memset(errmsg,   0, CODETAGGER_STR_LEN);
   memset(tagName,  0, CODETAGGER_STR_LEN);

   /* reads tag file */
   if (!(data = codetagger_get_file_contents(cnf->tagFile)))
      return(-1);

   /* counts number of lines in tag file */
   for(lineCount = 0; data[lineCount]; lineCount++);

   /* compiles regex used to locate tags */
   codetagger_debug(5, _("   compiling regular expression for tags\n"));
   strncat(regstr,       "^",              CODETAGGER_STR_LEN);
   codetagger_escape_string(regstr, cnf->leftTagString,  CODETAGGER_STR_LEN);
   strncat(regstr,       "([_[:blank:][:alnum:]]+)START",   CODETAGGER_STR_LEN);
   codetagger_escape_string(regstr, cnf->rightTagString, CODETAGGER_STR_LEN);
   if ((err = regcomp(&regex, regstr, REG_EXTENDED|REG_ICASE)))
   {
      regerror(err, &regex, errmsg, CODETAGGER_STR_LEN-1);
      printf("regex error: %s\n", errmsg);
      return(-1);
   };
   codetagger_debug(8, _("   compiled regular expression \"%s\"\n"), regstr);

   /* loops through looking for tags */
   codetagger_debug(3, _("creating tag index from \"%s\"\n"), cnf->tagFile);
   for(i = 0; i < lineCount; i++)
   {
      if (!(err = regexec(&regex, data[i], 5, match, 0)))
      {
         data[i][(int)match[1].rm_eo] = '\0';
         strncpy(tagName, &data[i][(int)match[1].rm_so], CODETAGGER_STR_LEN-1);
         i++;
         if ((i = codetagger_process_tag(tagName, data, lineCount, i, cnf)) == -1)
         {
            codetagger_free_filedata(data);
            regfree(&regex);
            return(-1);
         };
         cnf->tagCount++;
      };
   };

   /* frees resources */
   codetagger_debug(7, _("   freeing file buffer\n"));
   codetagger_free_filedata(data);
   codetagger_debug(7, _("   freeing regular expressions\n"));
   regfree(&regex);

   /* ends function */
   return(0);
}


/// reads file into an array
/// @param[in]  file  file name of file to process
char ** codetagger_get_file_contents(const char * file)
{
   /* declares local vars */
   int            i;
   int            fd;
   int            len;
   int            count;
   char         * buff;
   char        ** lines;
   struct stat    sb;

   /* prints debug information */
   codetagger_debug(2, _("processing file \"%s\"\n"), file);

   /* stat file */
   codetagger_debug(6, _("   inspecting file\n"));
   if ((stat(file, &sb)) == -1)
   {
      perror(PROGRAM_NAME ": stat()");
      return(NULL);
   };

   /* opens file for reading */
   codetagger_debug(6, _("   opening file\n"));
   if ((fd = open(file, O_RDONLY)) == -1)
   {
      perror(PROGRAM_NAME ": open()");
      return(NULL);
   };

   /* allocates memory for buffer */
   codetagger_debug(7, _("   allocating memory for file buffer\n"));
   if (!(buff = (char *) malloc(sb.st_size + 1)))
   {
      fprintf(stderr, PROGRAM_NAME ": out of virtual memory\n");
      close(fd);
      return(NULL);
   };

   /* reads file into buffer */
   codetagger_debug(6, _("   reading file\n"));
   if ((len = read(fd, buff, sb.st_size)) == -1)
   {
      perror(PROGRAM_NAME ": read()");
      close(fd);
      return(NULL);
   };
   buff[len] = '\0';

   /* closes file */
   codetagger_debug(6, _("   closing file\n"));
   close(fd);

   /* counts and terminates lines */
   codetagger_debug(5, _("   analyzing file contents\n"));
   count = 0;
   for(i = 0; i < len; i++)
   {
      if (buff[i] == '\n')
      {
         count++;
         buff[i] = '\0';
      };
   };

   /* allocates memory for line array */
   codetagger_debug(7, _("   allocating memory for file index\n"));
   count++;
   if (!(lines = (char **) malloc(sizeof(char *) * count)))
   {
      perror(PROGRAM_NAME ": read()");
      free(buff);
      return(NULL);
   };

   /* places lines in array */
   codetagger_debug(5, _("   indexing file's contents\n"));
   lines[0] = buff;
   count = 0;
   for(i = 0; i < len; i++)
   {
      if (buff[i] == '\0')
      {
         count++;
         lines[count] = &buff[i+1];
      };
   };
   lines[count] = NULL;

   /* ends function */
   return(lines);
}


/// main statement
/// @param[in]  argc  number of arguments passed to program
/// @param[in]  argv  array of arguments passed to program
int main(int argc, char * argv[])
{
   /* declares local vars */
   int           c;
   int           i;
   int           opt_index;
   Config        cnf;

   /* getopt options */
   static char   short_opt[] = "f:hl:r:tvV";
   static struct option long_opt[] =
   {
      {"continue",      no_argument, 0, 'c'},
      {"help",          no_argument, 0, 'h'},
      {"silent",        no_argument, 0, 'q'},
      {"quiet",         no_argument, 0, 'q'},
      {"test",          no_argument, 0, 't'},
      {"verbose",       no_argument, 0, 'v'},
      {"version",       no_argument, 0, 'V'},
      {NULL,            0,           0, 0  }
   };

#ifdef HAVE_GETTEXT
   setlocale (LC_ALL, "");
   bindtextdomain (PACKAGE, LOCALEDIR);
   textdomain (PACKAGE);
#endif

   /* initialize variables */
   memset(&cnf, 0, sizeof(Config));
   cnf.leftTagString  = "@";
   cnf.rightTagString = "@";
   cnf.tagFile        = NULL;
   opt_index          = 0;

   /* loops through arguments */
   while((c = getopt_long(argc, argv, short_opt, long_opt, &opt_index)) != -1)
   {
      switch(c)
      {
         case -1:	/* no more arguments */
         case 0:	/* long options toggles */
            break;
         case 'a':
            cnf.opts |= CODETAGGER_OPT_HIDDEN;
            break;
         case 'c':
            cnf.opts |= CODETAGGER_OPT_CONTINUE;
            break;
         case 'd':
            cnf.opts |= CODETAGGER_OPT_DEBUG;
            break;
         case 'f':
            cnf.opts |= CODETAGGER_OPT_FORCE;
            break;
         case 'h':
            codetagger_usage();
            return(0);
         case 'i':
            cnf.tagFile = optarg;
            break;
         case 'L':
            cnf.opts |= CODETAGGER_OPT_LINKS;
            break;
         case 'l':
            cnf.leftTagString = optarg;
            break;
         case 'q':
            cnf.opts |= CODETAGGER_OPT_QUIET;
            break;
         case 'R':
            cnf.opts |= CODETAGGER_OPT_RECURSE;
            break;
         case 'r':
            cnf.rightTagString = optarg;
            break;
         case 't':
            cnf.opts |= CODETAGGER_OPT_TEST;
            break;
         case 'V':
            codetagger_version();
            return(0);
         case 'v':
            cnf.opts |= CODETAGGER_OPT_VERBOSE;
            break;
         case '?':
            fprintf(stderr, _("Try `%s --help' for more information.\n"), PROGRAM_NAME);
            return(1);
         default:
            fprintf(stderr, _("%s: unrecognized option `--%c'\n"
                  "Try `%s --help' for more information.\n"
               ),  PROGRAM_NAME, c, PROGRAM_NAME
            );
            return(1);
      };
   };

   /* verifies arguments were passed on the command line */
   if ((optind == argc) || (optind == 1))
   {
      codetagger_usage();
      return(1);
   };
   if (!(cnf.tagFile ))
   {
      fprintf(stderr, _("%s: missing required option `-f'\n"
            "Try `%s --help' for more information.\n"
         ), PROGRAM_NAME, PROGRAM_NAME
      );
      return(1);
   };

   /* parses tag file */
   if ((generate_taglist(&cnf)))
      return(0);
   if (!(cnf.tagCount))
   {
      fprintf(stderr, _(PROGRAM_NAME ": no tag definitions were found in \"%s\"\n"), cnf.tagFile);
      return(1);
   };

   /* loops through files to be tagged */
   for(i = optind; i < argc; i++)
      expand_tags(argv[i], &cnf);

   /* frees memory */
   codetagger_free_taglist(cnf.tagList);

   /* ends function */
   return(0);
}


/// displays usage
void codetagger_usage(void)
{
   // TRANSLATORS: The following strings provide usage for command. These
   // strings are displayed if the program is passed `--help' on the command
   // line. The two strings referenced are: PROGRAM_NAME, and
   // PACKAGE_BUGREPORT
   printf(_("Usage: %s [OPTIONS] files\n"
         "  -a                        include hidden files\n"
         "  -c                        continue on error\n"
         "  -d                        enter debug mode\n"
         "  -f                        force writes\n"
         "  -h, --help                print this help and exit\n"
         "  -i file                   file containing tags\n"
         "  -L                        follow symbolic links\n"
         "  -l str                    left enclosing string for tags\n"
         "  -q, --quiet, --silent     do not print messages\n"
         "  -r str                    right enclosing string for tags\n"
         "  -R                        recursively follow directories\n"
         "  -t, --test                show what would be done\n"
         "  -v, --verbose             print verbose messages\n"
         "  -V, --version             print version number and exit\n"
         "\n"
         "Report bugs to <%s>.\n"
      ), PROGRAM_NAME, PACKAGE_BUGREPORT
   );
   return;
}


/// displays version information
void codetagger_version(void)
{
   // TRANSLATORS: The following strings provide version and copyright
   // information if the program is passed --version on the command line.
   // The three strings referenced are: PROGRAM_NAME, PACKAGE_NAME,
   // PACKAGE_VERSION.
   printf(_("%s (%s) %s\n"
         "Written by David M. Syzdek.\n"
         "\n"
         "%s\n"
         "This is free software; see the source for copying conditions.  There is NO\n"
         "warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n"
      ), PROGRAM_NAME, PACKAGE_NAME, PACKAGE_VERSION, PACKAGE_COPYRIGHT
   );
   return;
}


/// processes tag from tag file
/// @param[in]  tagname   name of tag to process
/// @param[in]  data      data from which to extract the tag
/// @param[in]  len       length of data array
/// @param[in]  pos       current position within the data
/// @param[in]  taglistp  pointer to array of tags
int codetagger_process_tag(char * tagName, char ** data, int len, int pos, Config * cnf)
{
   /* declares local vars */
   int           i;
   int           err;
   int           count;
   int           tagCount;
   int           line_count;
   TagData     * tag;
   void        * ptr;
   char          regstr[CODETAGGER_STR_LEN];
   char          errmsg[CODETAGGER_STR_LEN];
   regmatch_t    match[5];

   /* checks arguments */
   if (!(tagName))
      return(-1);
   if (!(data))
      return(-1);
   if (!(cnf))
      return(-1);
      
   memset(regstr, 0, CODETAGGER_STR_LEN);
   memset(errmsg, 0, CODETAGGER_STR_LEN);

   /* prints debugging information */
   codetagger_debug(5, _("   indexing tag \"%s\"\n"), tagName);

   /* counts tags in taglist */
   tagCount = 0;
   if (cnf->tagList)
      for(tagCount = 0; cnf->tagList[tagCount]; tagCount++);

   /* allocates memory for tag */
   tagCount++;
   if (!(tag = (TagData *) malloc(sizeof(TagData))))
   {
      fprintf(stderr, _(PROGRAM_NAME ": out of virtual memory\n"));
      return(-1);
   };
   memset(tag, 0, sizeof(TagData));

   /* allocates memory for array */
   if (!(ptr = realloc(cnf->tagList, sizeof(TagData) * (tagCount + 1))))
   {
      fprintf(stderr, _(PROGRAM_NAME ": out of virtual memory\n"));
      codetagger_free_tag(tag);
      return(-1);
   };
   cnf->tagList             = ptr;
   cnf->tagList[tagCount-1] = NULL;
   cnf->tagList[tagCount]   = NULL;

   /* saves tag name */
   if (!(tag->name = strdup(tagName)))
   {
      fprintf(stderr, _(PROGRAM_NAME ": out of virtual memory\n"));
      codetagger_free_tag(tag);
      return(-1);
   };

   /* compiles regular expression */
   codetagger_debug(5, _("   compiling regular expression\n"));
   codetagger_escape_string(regstr, cnf->leftTagString,  CODETAGGER_STR_LEN);
   strncat(regstr,       "(",                 CODETAGGER_STR_LEN);
   strncat(regstr,       tagName,             CODETAGGER_STR_LEN);
   strncat(regstr,       "END",               CODETAGGER_STR_LEN);
   strncat(regstr,       ")",                 CODETAGGER_STR_LEN);
   codetagger_escape_string(regstr, cnf->rightTagString, CODETAGGER_STR_LEN);
   if ((err = regcomp(&tag->regex, regstr, REG_EXTENDED|REG_ICASE)))
   {
      regerror(err, &tag->regex, errmsg, CODETAGGER_STR_LEN-1);
      printf("regex error: %s\n", errmsg);
      codetagger_free_tag(tag);
      return(-1);
   };

   /* loops through file */
   err = 1;
   for(count = pos; ((count < len) && (err)); count++)
      err = regexec(&tag->regex, data[count], 5, match, 0);

   /* checks for error */
   if (err)
   {
      codetagger_free_tag(tag);
      return(-1);
   };

   /* calculate number of lines in tag */
   line_count = count - pos - 1;

   /* allocate memory for tag contents */
   if (!(tag->contents = (char **) malloc(sizeof(char *) * (line_count + 1))))
   {
      codetagger_free_tag(tag);
      return(-1);
   };
   memset(tag->contents, 0, (sizeof(char *) * (line_count + 1)));

   /* copies lines into tag */
   for(i = pos; i < (pos + line_count); i++)
   {
      if (!(tag->contents[i-pos] = strdup(data[i])))
      {
         codetagger_free_tag(tag);
         return(-1);
      };
   };

   /* saves tag */
   cnf->tagList[tagCount-1] = tag;

   /* ends function */
   return(i);
}


/* end of source file */
