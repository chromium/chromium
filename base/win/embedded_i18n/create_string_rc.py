#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generates .h and .rc files for strings extracted from a .grd file.

This script generates an rc file and header (NAME.{rc,h}) to be included in
a build target. The rc file includes translations for strings pulled from the
given .grd file(s) and their corresponding localized .xtb files.

To specify strings that will be extracted, the script pointed to by the
argument "extract-datafile" should contain one or both of the following global
variables:

STRING_IDS is a list of strings IDs we want to import from the .grd files and
include in the generated RC file. These strings are universal for all brands.

MODE_SPECIFIC_STRINGS: is a dictionary of strings for which there are brand
specific values. This mapping provides brand- and mode-specific string ids for a
given input id as described here:

{
  resource_id_1: {  # A resource ID for use with GetLocalizedString.
    brand_1: [  # 'google_chrome', for example.
      string_id_1,  # Strings listed in order of the brand's modes, as
      string_id_2,  # specified in install_static::InstallConstantIndex.
      ...
      string_id_N,
    ],
    brand_2: [  # 'chromium', for example.
      ...
    ],
  },
  resource_id_2:  ...
}

Note: MODE_SPECIFIC_STRINGS cannot be specified if STRING_IDS is not specified.

"""

# The generated header file includes IDs for each string, but also has values to
# allow getting a string based on a language offset.  For example, the header
# file looks like this:
#
# #define IDS_L10N_OFFSET_AR 0
# #define IDS_L10N_OFFSET_BG 1
# #define IDS_L10N_OFFSET_CA 2
# ...
# #define IDS_L10N_OFFSET_ZH_TW 41
#
# #define IDS_MY_STRING_AR 1600
# #define IDS_MY_STRING_BG 1601
# ...
# #define IDS_MY_STRING_BASE IDS_MY_STRING_AR
#
# This allows us to lookup an an ID for a string by adding IDS_MY_STRING_BASE
# and IDS_L10N_OFFSET_* for the language we are interested in.
#

from __future__ import print_function

import argparse
import glob
import io
import os
import sys
from xml import sax

BASEDIR = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(1, os.path.join(BASEDIR, '../../../tools/grit'))
sys.path.insert(2, os.path.join(BASEDIR, '../../../tools/python'))

from grit.extern import tclib

class GrdHandler(sax.handler.ContentHandler):
  """Extracts selected strings from a .grd file.

  Attributes:
    messages: A dict mapping string identifiers to their corresponding messages.
    referenced_xtb_files: A list of all xtb files referenced inside the .grd
      file.
  """
  def __init__(self, string_id_set):
    """Constructs a handler that reads selected strings from a .grd file.

    The dict attribute |messages| is populated with the strings that are read.

    Args:
      string_id_set: An optional set of message identifiers to extract; all
      messages are extracted if empty.
    """
    sax.handler.ContentHandler.__init__(self)
    self.messages = {}
    self.referenced_xtb_files = []
    self.__id_set = string_id_set
    self.__message_name = None
    self.__element_stack = []
    self.__text_scraps = []
    self.__characters_callback = None

  def startElement(self, name, attrs):
    self.__element_stack.append(name)
    if name == 'message':
      self.__OnOpenMessage(attrs.getValue('name'))
    elif name == 'file':
      parent = self.__element_stack[-2]
      if parent == 'translations':
        self.__OnAddXtbFile(attrs.getValue('path'))

  def endElement(self, name):
    popped = self.__element_stack.pop()
    assert popped == name
    if name == 'message':
      self.__OnCloseMessage()

  def characters(self, content):
    if self.__characters_callback:
      self.__characters_callback(self.__element_stack[-1], content)

  def __IsExtractingMessage(self):
    """Returns True if a message is currently being extracted."""
    return self.__message_name is not None

  def __OnOpenMessage(self, message_name):
    """Invoked at the start of a <message> with message's name."""
    assert not self.__IsExtractingMessage()
    self.__message_name = (message_name if (not (self.__id_set) or
                           message_name in self.__id_set)
                           else None)
    if self.__message_name:
      self.__characters_callback = self.__OnMessageText

  def __OnMessageText(self, containing_element, message_text):
    """Invoked to handle a block of text for a message."""
    if message_text and (containing_element == 'message' or
                         containing_element == 'ph'):
      self.__text_scraps.append(message_text)

  def __OnCloseMessage(self):
    """Invoked at the end of a message."""
    if self.__IsExtractingMessage():
      self.messages[self.__message_name] = ''.join(self.__text_scraps).strip()
      self.__message_name = None
      self.__text_scraps = []
      self.__characters_callback = None

  def __OnAddXtbFile(self, xtb_file_path):
    """Adds the xtb file path of a 'file'."""
    if os.path.splitext(xtb_file_path)[1].lower() == '.xtb':
      self.referenced_xtb_files.append(xtb_file_path)

class XtbHandler(sax.handler.ContentHandler):
  """Extracts selected translations from an .xtd file.

  Populates the |lang| and |translations| attributes with the language and
  selected strings of an .xtb file. Instances may be re-used to read the same
  set of translations from multiple .xtb files.

  Attributes:
    translations: A mapping of translation ids to strings.
    lang: The language parsed from the .xtb file.
  """
  def __init__(self, translation_ids):
    """Constructs an instance to parse the given strings from an .xtb file.

    Args:
      translation_ids: a mapping of translation ids to their string
        identifiers list for the translations to be extracted.
    """
    sax.handler.ContentHandler.__init__(self)
    self.lang = None
    self.translations = None
    self.__translation_ids = translation_ids
    self.__element_stack = []
    self.__string_ids = None
    self.__text_scraps = []
    self.__characters_callback = None

  def startDocument(self):
    # Clear the lang and translations since a new document is being parsed.
    self.lang = ''
    self.translations = {}

  def startElement(self, name, attrs):
    self.__element_stack.append(name)
    # translationbundle is the document element, and hosts the lang id.
    if len(self.__element_stack) == 1:
      assert name == 'translationbundle'
      self.__OnLanguage(attrs.getValue('lang'))
    if name == 'translation':
      self.__OnOpenTranslation(attrs.getValue('id'))

  def endElement(self, name):
    popped = self.__element_stack.pop()
    assert popped == name
    if name == 'translation':
      self.__OnCloseTranslation()

  def characters(self, content):
    if self.__characters_callback:
      self.__characters_callback(self.__element_stack[-1], content)

  def __OnLanguage(self, lang):
    self.lang = lang.replace('-', '_').upper()

  def __OnOpenTranslation(self, translation_id):
    assert self.__string_ids is None
    self.__string_ids = self.__translation_ids.get(translation_id)
    if self.__string_ids:
      self.__characters_callback = self.__OnTranslationText

  def __OnTranslationText(self, containing_element, message_text):
    if message_text and containing_element == 'translation':
      self.__text_scraps.append(message_text)

  def __OnCloseTranslation(self):
    if self.__string_ids:
      translated_string = ''.join(self.__text_scraps).strip()
      for string_id in self.__string_ids:
        self.translations[string_id] = translated_string
      self.__string_ids = None
      self.__text_scraps = []
      self.__characters_callback = None


class StringRcMaker(object):
  """Makes .h and .rc files containing strings and translations."""
  def __init__(self, inputs, expected_xtb_input_files, header_file, rc_file,
    brand, first_resource_id, string_ids_to_extract, mode_specific_strings):
    """Constructs a maker.

    Args:
      inputs: A list of (grd_file, xtb_dir) pairs containing the source data.
      expected_xtb_input_files: A list of xtb files that are expected to exist
        in the inputs folders. If there is a discrepency between what exists
        and what is expected the script will fail.
      header_file: The location of the header file to write containing all the
        defined string IDs.
      rc_file: The location of the rc file to write containing all the string
        resources.
      brand: The brand to check against when extracting mode-specific strings.
      first_resource_id: The starting ID for the generated string resources.
      string_ids_to_extract: The IDs of strings we want to import from the .grd
        files and include in the generated RC file. These strings are universal
        for all brands.
      mode_specific_strings: A dictionary of strings that have conditional
        values based on the brand's install mode. Refer to the documentation at
        the top of this file for more information on the format of the
        dictionary.
    """
    self.inputs = inputs
    self.expected_xtb_input_files = expected_xtb_input_files
    self.expected_xtb_input_files.sort()
    self.header_file = header_file
    self.rc_file = rc_file
    self.brand = brand
    self.first_resource_id = first_resource_id;
    self.string_id_set = set(string_ids_to_extract)
    self.mode_specific_strings = mode_specific_strings
    self.__AddModeSpecificStringIds()

  def MakeFiles(self):
    translated_strings = self.__ReadSourceAndTranslatedStrings()
    self.__WriteRCFile(translated_strings)
    self.__WriteHeaderFile(translated_strings)

  class __TranslationData(object):
    """A container of information about a single translation."""
    def __init__(self, resource_id_str, language, translation):
      self.resource_id_str = resource_id_str
      self.language = language
      self.translation = translation

    def __lt__(self, other):
      """Allow __TranslationDatas to be sorted by id then by language."""
      return (self.resource_id_str, self.language) < (other.resource_id_str,
                                                      other.language)

  def __AddModeSpecificStringIds(self):
    """Adds the mode-specific strings for all of the current brand's install
    modes to self.string_id_set."""
    for string_id, brands in self.mode_specific_strings.items():
      brand_strings = brands.get(self.brand)
      if not brand_strings:
        raise RuntimeError(
            'No strings declared for brand \'%s\' in MODE_SPECIFIC_STRINGS for '
            'message %s' % (self.brand, string_id))
      self.string_id_set.update(brand_strings)

  def __ReadSourceAndTranslatedStrings(self):
    """Reads the source strings and translations from all inputs."""
    translated_strings = []
    all_xtb_files = []
    for grd_file, xtb_dir in self.inputs:
      # Get the name of the grd file sans extension.
      source_name = os.path.splitext(os.path.basename(grd_file))[0]
      # Compute a glob for the translation files.
      xtb_pattern = os.path.join(os.path.dirname(grd_file), xtb_dir,
                                 '%s*.xtb' % source_name)
      local_xtb_files = [x.replace('\\', '/') for x in glob.glob(xtb_pattern)]
      all_xtb_files.extend(local_xtb_files)
      translated_strings.extend(
        self.__ReadSourceAndTranslationsFrom(grd_file, local_xtb_files))
    translated_strings.sort()
    all_xtb_files.sort()

    if self.expected_xtb_input_files != all_xtb_files:
      extra = list(set(all_xtb_files) - set(self.expected_xtb_input_files))
      missing = list(set(self.expected_xtb_input_files) - set(all_xtb_files))
      error = '''Asserted file list does not match.

Expected input files:
{}
Actual input files:
{}
Missing input files:
{}
Extra input files:
{}
'''
      print(error.format('\n'.join(self.expected_xtb_input_files),
                         '\n'.join(all_xtb_files), '\n'.join(missing),
                         '\n'.join(extra)))
      sys.exit(1)
    return translated_strings

  def __ReadSourceAndTranslationsFrom(self, grd_file, xtb_files):
    """Reads source strings and translations for a .grd file.

    Reads the source strings and all available translations for the messages
    identified by self.string_id_set (or all the messages if self.string_id_set
    is empty). The source string is used where translations are missing.

    Args:
      grd_file: Path to a .grd file.
      xtb_files: List of paths to .xtb files.

    Returns:
      An unsorted list of __TranslationData instances.
    """
    sax_parser = sax.make_parser()

    # Read the source (en-US) string from the .grd file.
    grd_handler = GrdHandler(self.string_id_set)
    sax_parser.setContentHandler(grd_handler)
    sax_parser.parse(grd_file)
    source_strings = grd_handler.messages

    grd_file_path = os.path.dirname(grd_file)
    source_xtb_files = []
    for xtb_file in grd_handler.referenced_xtb_files:
      relative_xtb_file_path = (
        os.path.join(grd_file_path, xtb_file).replace('\\', '/'))
      source_xtb_files.append(relative_xtb_file_path)
    missing_xtb_files = list(set(source_xtb_files) - set(xtb_files))

    # Manually put the source strings as en-US in the list of translated
    # strings.
    translated_strings = []
    for string_id, message_text in source_strings.items():
      translated_strings.append(self.__TranslationData(string_id,
                                                       'EN_US',
                                                       message_text))

    # Generate the message ID for each source string to correlate it with its
    # translations in the .xtb files. Multiple source strings may have the same
    # message text; hence the message id is mapped to a list of string ids
    # instead of a single value.
    translation_ids = {}
    for (string_id, message_text) in source_strings.items():
      message_id = tclib.GenerateMessageId(message_text)
      translation_ids.setdefault(message_id, []).append(string_id);

    # Track any xtb files that appear in the xtb folder but are not present in
    # the grd file.
    extra_xtb_files = []
    # Gather the translated strings from the .xtb files. Use the en-US string
    # for any message lacking a translation.
    xtb_handler = XtbHandler(translation_ids)
    sax_parser.setContentHandler(xtb_handler)
    for xtb_filename in xtb_files:
      if not xtb_filename in source_xtb_files:
        extra_xtb_files.append(xtb_filename)
      sax_parser.parse(xtb_filename)
      for string_id, message_text in source_strings.items():
        translated_string = xtb_handler.translations.get(string_id,
                                                         message_text)
        translated_strings.append(self.__TranslationData(string_id,
                                                         xtb_handler.lang,
                                                         translated_string))
    if missing_xtb_files or extra_xtb_files:
      if missing_xtb_files:
        missing_error = ("There were files that were found in the .grd file "
                         "'{}' but do not exist on disk:\n{}")
        print(missing_error.format(grd_file, '\n'.join(missing_xtb_files)))

      if extra_xtb_files:
        extra_error = ("There were files that exist on disk but were not found "
                       "in the .grd file '{}':\n{}")
        print(extra_error.format(grd_file, '\n'.join(extra_xtb_files)))

      sys.exit(1)
    return translated_strings

  def __WriteRCFile(self, translated_strings):
    """Writes a resource file with the strings provided in |translated_strings|.
    """
    HEADER_TEXT = (
      u'#include "%s"\n\n'
      u'STRINGTABLE\n'
      u'BEGIN\n'
      ) % os.path.basename(self.header_file)

    FOOTER_TEXT = (
      u'END\n'
    )

    with io.open(self.rc_file,
                 mode='w',
                 encoding='utf-16',
                 newline='\n') as outfile:
      outfile.write(HEADER_TEXT)
      for translation in translated_strings:
        # Escape special characters for the rc file.
        escaped_text = (translation.translation.replace('"', '""')
                       .replace('\t', '\\t')
                       .replace('\n', '\\n'))
        outfile.write(u'  %s "%s"\n' %
                      (translation.resource_id_str + '_' + translation.language,
                       escaped_text))
      outfile.write(FOOTER_TEXT)

  def __WriteHeaderFile(self, translated_strings):
    """Writes a .h file with resource ids."""
    # TODO(grt): Stream the lines to the file rather than building this giant
    # list of lines first.
    lines = []
    do_languages_lines = ['\n#define DO_LANGUAGES']
    installer_string_mapping_lines = ['\n#define DO_STRING_MAPPING']
    do_mode_strings_lines = ['\n#define DO_MODE_STRINGS']

    # Write the values for how the languages ids are offset.
    seen_languages = set()
    offset_id = 0
    for translation_data in translated_strings:
      lang = translation_data.language
      if lang not in seen_languages:
        seen_languages.add(lang)
        lines.append('#define IDS_L10N_OFFSET_%s %s' % (lang, offset_id))
        do_languages_lines.append('  HANDLE_LANGUAGE(%s, IDS_L10N_OFFSET_%s)'
                                  % (lang.replace('_', '-').lower(), lang))
        offset_id += 1
      else:
        break

    # Write the resource ids themselves.
    resource_id = self.first_resource_id
    for translation_data in translated_strings:
      lines.append('#define %s %s' % (translation_data.resource_id_str + '_' +
                                      translation_data.language,
                                      resource_id))
      resource_id += 1

    # Handle mode-specific strings.
    for string_id, brands in self.mode_specific_strings.items():
      # Populate the DO_MODE_STRINGS macro.
      brand_strings = brands.get(self.brand)
      if not brand_strings:
        raise RuntimeError(
            'No strings declared for brand \'%s\' in MODE_SPECIFIC_STRINGS for '
            'message %s' % (self.brand, string_id))
      do_mode_strings_lines.append(
        '  HANDLE_MODE_STRING(%s_BASE, %s)'
        % (string_id, ', '.join([ ('%s_BASE' % s) for s in brand_strings])))

    # Generate defines for the specific strings to extract or take all of the
    # strings found in the translations.
    if self.string_id_set:
      string_ids_to_write = self.string_id_set;
    else:
      string_ids_to_write = {t.resource_id_str for t in translated_strings}

    # Write out base ID values.
    for string_id in sorted(string_ids_to_write):
      lines.append('#define %s_BASE %s_%s' % (string_id,
                                              string_id,
                                              translated_strings[0].language))
      installer_string_mapping_lines.append('  HANDLE_STRING(%s_BASE, %s)'
                                            % (string_id, string_id))

    with open(self.header_file, 'w') as outfile:
      outfile.write('\n'.join(lines))
      outfile.write('\n#ifndef RC_INVOKED')
      outfile.write(' \\\n'.join(do_languages_lines))
      outfile.write(' \\\n'.join(installer_string_mapping_lines))
      outfile.write(' \\\n'.join(do_mode_strings_lines))
      # .rc files must end in a new line
      outfile.write('\n#endif  // ndef RC_INVOKED\n')


def BuildArgumentParser():
  parser = argparse.ArgumentParser(
    description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
  parser.add_argument('-b',
                      help='identifier of the browser brand (e.g., chromium).'
                      'This argument is mandatory if the module file included'
                      'by --extract-datafile contains MODE_SPECIFIC_STRINGS',
                      dest='brand')
  parser.add_argument('-i', action='append',
                      required=True,
                      help='path to .grd file',
                      dest='input_grd_files')
  parser.add_argument('-r', action='append',
                      required=True,
                      help='relative path to .xtb dir for each .grd file',
                      dest='input_xtb_relative_paths')
  parser.add_argument('-x', action='append',
                      required=True,
                      help='expected xtb input files to read',
                      dest='expected_xtb_input_files')
  parser.add_argument('--header-file',
                      required=True,
                      help='path to generated .h file to write',
                      dest='header_file')
  parser.add_argument('--rc-file',
                      required=True,
                      help='path to generated .rc file to write',
                      dest='rc_file')
  parser.add_argument('--first-resource-id',
                      type=int,
                      required=True,
                      help='first id for the generated string resources',
                      dest='first_resource_id')
  parser.add_argument('--extract-datafile',
                      help='the python file execute that will define the '
                      'specific strings to extract from the source .grd file.'
                      'The module should contain a global array STRING_IDS '
                      'that specifies which string IDs need to be extracted '
                      '(if no global member by that name exists, then all the '
                      'strings are extracted). It may also optionally contain '
                      'a dictionary MODE_SPECIFIC_STRINGS which defines the '
                      'mode-specific strings to use for a given brand that is '
                      'extracted.',
                      dest='extract_datafile')

  return parser


def main():
  parser = BuildArgumentParser()
  args = parser.parse_args()
  # Extract all the strings from the given grd by default.
  string_ids_to_extract = []
  mode_specific_strings = {}

  # Check to see if an external module containing string extraction information
  # was specified.
  extract_datafile = args.extract_datafile
  if extract_datafile:
    datafile_locals = dict();
    exec(open(extract_datafile).read(), globals(), datafile_locals)
    if 'STRING_IDS' in datafile_locals:
      string_ids_to_extract = datafile_locals['STRING_IDS']
    if 'MODE_SPECIFIC_STRINGS' in datafile_locals:
      if not string_ids_to_extract:
        parser.error('MODE_SPECIFIC_STRINGS was specified in file ' +
          extract_datafile + ' but there were no specific STRING_IDS '
          'specified for extraction')
      mode_specific_strings = datafile_locals['MODE_SPECIFIC_STRINGS']

  brand = args.brand
  if brand:
    if not mode_specific_strings:
      parser.error('A brand was specified (' + brand + ') but no mode '
        'specific strings were given.')
    valid_brands = [b for b in
      next(iter(mode_specific_strings.values())).keys()]
    if not brand in valid_brands:
      parser.error('A brand was specified (' + brand + ') but it is not '
        'a valid brand [' + ', '.join(valid_brands) + '].')
  elif mode_specific_strings:
    parser.error('MODE_SPECIFIC_STRINGS were specified but no brand was '
      'given.')

  grd_files = args.input_grd_files
  xtb_relative_paths = args.input_xtb_relative_paths

  if len(grd_files) != len(xtb_relative_paths):
    parser.error('Mismatch in number of grd files ({}) and xtb relative '
                 'paths ({})'.format(len(grd_files), len(xtb_relative_paths)))

  inputs = zip(grd_files, xtb_relative_paths)

  StringRcMaker(inputs, args.expected_xtb_input_files, args.header_file,
    args.rc_file,  brand, args.first_resource_id, string_ids_to_extract,
    mode_specific_strings).MakeFiles()
  return 0

if '__main__' == __name__:
  sys.exit(main())
