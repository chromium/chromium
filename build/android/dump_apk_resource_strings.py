#!/usr/bin/env python
# encoding: utf-8
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A script to parse and dump localized strings in resource.arsc files."""

from __future__ import print_function

import argparse
import collections
import contextlib
import cProfile
import os
import re
import subprocess
import sys
import zipfile

# pylint: disable=bare-except

# Assuming this script is located under build/android, try to import
# build/android/gyp/bundletool.py to get the default path to the bundletool
# jar file. If this fail, using --bundletool-path will be required to parse
# bundles, allowing this script to be relocated or reused somewhere else.
try:
  sys.path.insert(0, os.path.join(os.path.dirname(__file__), 'gyp'))
  import bundletool

  _DEFAULT_BUNDLETOOL_PATH = bundletool.BUNDLETOOL_JAR_PATH
except:
  _DEFAULT_BUNDLETOOL_PATH = None

# Try to get the path of the aapt build tool from catapult/devil.
try:
  import devil_chromium  # pylint: disable=unused-import
  from devil.android.sdk import build_tools
  _AAPT_DEFAULT_PATH = build_tools.GetPath('aapt')
except:
  _AAPT_DEFAULT_PATH = None


def AutoIndentStringList(lines, indentation=2):
  """Auto-indents a input list of text lines, based on open/closed braces.

  For example, the following input text:

    'Foo {',
    'Bar {',
    'Zoo',
    '}',
    '}',

  Will return the following:

    'Foo {',
    '  Bar {',
    '    Zoo',
    '  }',
    '}',

  The rules are pretty simple:
    - A line that ends with an open brace ({) increments indentation.
    - A line that starts with a closing brace (}) decrements it.

  The main idea is to make outputting structured text data trivial,
  since it can be assumed that the final output will be passed through
  this function to make it human-readable.

  Args:
    lines: an iterator over input text lines. They should not contain
      line terminator (e.g. '\n').
  Returns:
    A new list of text lines, properly auto-indented.
  """
  margin = ''
  result = []
  # NOTE: Intentional but significant speed optimizations in this function:
  #   - |line and line[0] == <char>| instead of |line.startswith(<char>)|.
  #   - |line and line[-1] == <char>| instead of |line.endswith(<char>)|.
  for line in lines:
    if line and line[0] == '}':
      margin = margin[:-indentation]
    result.append(margin + line)
    if line and line[-1] == '{':
      margin += ' ' * indentation

  return result


# pylint: disable=line-too-long

# NOTE: aapt dump will quote the following characters only: \n, \ and "
# see https://android.googlesource.com/platform/frameworks/base/+/master/libs/androidfw/ResourceTypes.cpp#7270

# pylint: enable=line-too-long


def UnquoteString(s):
  """Unquote a given string from aapt dump.

  Args:
    s: An UTF-8 encoded string that contains backslashes for quotes, as found
      in the output of 'aapt dump resources --values'.
  Returns:
    The unquoted version of the input string.
  """
  if not '\\' in s:
    return s

  result = ''
  start = 0
  size = len(s)
  while start < size:
    pos = s.find('\\', start)
    if pos < 0:
      break

    result += s[start:pos]
    count = 1
    while pos + count < size and s[pos + count] == '\\':
      count += 1

    result += '\\' * (count / 2)
    start = pos + count
    if count & 1:
      if start < size:
        ch = s[start]
        if ch == 'n':  # \n is the only non-printable character supported.
          ch = '\n'
        result += ch
        start += 1
      else:
        result += '\\'

  result += s[start:]
  return result


assert UnquoteString(r'foo bar') == 'foo bar'
assert UnquoteString(r'foo\nbar') == 'foo\nbar'
assert UnquoteString(r'foo\\nbar') == 'foo\\nbar'
assert UnquoteString(r'foo\\\nbar') == 'foo\\\nbar'
assert UnquoteString(r'foo\n\nbar') == 'foo\n\nbar'
assert UnquoteString(r'foo\\bar') == r'foo\bar'


def QuoteString(s):
  """Quote a given string for external output.

  Args:
    s: An input UTF-8 encoded string.
  Returns:
    A quoted version of the string, using the same rules as 'aapt dump'.
  """
  # NOTE: Using repr() would escape all non-ASCII bytes in the string, which
  # is undesirable.
  return s.replace('\\', r'\\').replace('"', '\\"').replace('\n', '\\n')


assert QuoteString(r'foo "bar"') == 'foo \\"bar\\"'
assert QuoteString('foo\nbar') == 'foo\\nbar'


def ReadStringMapFromRTxt(r_txt_path):
  """Read all string resource IDs and names from an R.txt file.

  Args:
    r_txt_path: Input file path.
  Returns:
    A {res_id -> res_name} dictionary corresponding to the string resources
    from the input R.txt file.
  """
  # NOTE: Typical line of interest looks like:
  # int string AllowedDomainsForAppsTitle 0x7f130001
  result = {}
  prefix = 'int string '
  with open(r_txt_path) as f:
    for line in f:
      line = line.rstrip()
      if line.startswith(prefix):
        res_name, res_id = line[len(prefix):].split(' ')
        result[int(res_id, 0)] = res_name
  return result


class ResourceStringValues(object):
  """Models all possible values for a named string."""

  def __init__(self):
    self.res_name = None
    self.res_values = {}

  def AddValue(self, res_name, res_config, res_value):
    """Add a new value to this entry.

    Args:
      res_name: Resource name. If this is not the first time this method
        is called with the same resource name, then |res_name| should match
        previous parameters for sanity checking.
      res_config: Config associated with this value. This can actually be
        anything that can be converted to a string.
      res_value: UTF-8 encoded string value.
    """
    if res_name is not self.res_name and res_name != self.res_name:
      if self.res_name is None:
        self.res_name = res_name
      else:
        # Sanity check: the resource name should be the same for all chunks.
        # Resource ID is redefined with a different name!!
        print('WARNING: Resource key ignored (%s, should be %s)' %
              (res_name, self.res_name))

    if self.res_values.setdefault(res_config, res_value) is not res_value:
      print('WARNING: Duplicate value definition for [config %s]: %s ' \
            '(already has %s)' % (
                res_config, res_value, self.res_values[res_config]))

  def ToStringList(self, res_id):
    """Convert entry to string list for human-friendly output."""
    values = sorted(
        [(str(config), value) for config, value in self.res_values.iteritems()])
    if res_id is None:
      # res_id will be None when the resource ID should not be part
      # of the output.
      result = ['name=%s count=%d {' % (self.res_name, len(values))]
    else:
      result = [
          'res_id=0x%08x name=%s count=%d {' % (res_id, self.res_name,
                                                len(values))
      ]
    for config, value in values:
      result.append('%-16s "%s"' % (config, QuoteString(value)))
    result.append('}')
    return result


class ResourceStringMap(object):
  """Convenience class to hold the set of all localized strings in a table.

  Usage is the following:
     1) Create new (empty) instance.
     2) Call AddValue() repeatedly to add new values.
     3) Eventually call RemapResourceNames() to remap resource names.
     4) Call ToStringList() to convert the instance to a human-readable
        list of strings that can later be used with AutoIndentStringList()
        for example.
  """

  def __init__(self):
    self._res_map = collections.defaultdict(ResourceStringValues)

  def AddValue(self, res_id, res_name, res_config, res_value):
    self._res_map[res_id].AddValue(res_name, res_config, res_value)

  def RemapResourceNames(self, id_name_map):
    """Rename all entries according to a given {res_id -> res_name} map."""
    for res_id, res_name in id_name_map.iteritems():
      if res_id in self._res_map:
        self._res_map[res_id].res_name = res_name

  def ToStringList(self, omit_ids=False):
    """Dump content to a human-readable string list.

    Note that the strings are ordered by their resource name first, and
    resource id second.

    Args:
      omit_ids: If True, do not put resource IDs in the result. This might
        be useful when comparing the outputs of two different builds of the
        same APK, or two related APKs (e.g. ChromePublic.apk vs Chrome.apk)
        where the resource IDs might be slightly different, but not the
        string contents.
    Return:
      A list of strings that can later be sent to AutoIndentStringList().
    """
    result = ['Resource strings (count=%d) {' % len(self._res_map)]
    res_map = self._res_map

    # A small function to compare two (res_id, values) tuples
    # by resource name first, then resource ID.
    def cmp_id_name(a, b):
      result = cmp(a[1].res_name, b[1].res_name)
      if result == 0:
        result = cmp(a[0], b[0])
      return result

    for res_id, _ in sorted(res_map.iteritems(), cmp=cmp_id_name):
      result += res_map[res_id].ToStringList(None if omit_ids else res_id)
    result.append('}  # Resource strings')
    return result


@contextlib.contextmanager
def ManagedOutput(output_file):
  """Create an output File object that will be closed on exit if necessary.

  Args:
    output_file: Optional output file path.
  Yields:
    If |output_file| is empty, this simply yields sys.stdout. Otherwise, this
    opens the file path for writing text, and yields its File object. The
    context will ensure that the object is always closed on scope exit.
  """
  close_output = False
  if output_file:
    output = open(output_file, 'wt')
    close_output = True
  else:
    output = sys.stdout
  try:
    yield output
  finally:
    if close_output:
      output.close()


@contextlib.contextmanager
def ManagedPythonProfiling(enable_profiling, sort_key='tottime'):
  """Enable Python profiling if needed.

  Args:
    enable_profiling: Boolean flag. True to enable python profiling.
    sort_key: Sorting key for the final stats dump.
  Yields:
    If |enable_profiling| is False, this yields False. Otherwise, this
    yields a new Profile instance just after enabling it. The manager
    ensures that profiling stops and prints statistics on scope exit.
  """
  pr = None
  if enable_profiling:
    pr = cProfile.Profile()
    pr.enable()
  try:
    yield pr
  finally:
    if pr:
      pr.disable()
      pr.print_stats(sort=sort_key)


def IsFilePathABundle(input_file):
  """Return True iff |input_file| holds an Android app bundle."""
  try:
    with zipfile.ZipFile(input_file) as input_zip:
      _ = input_zip.getinfo('BundleConfig.pb')
      return True
  except:
    return False


# Example output from 'bundletool dump resources --values' corresponding
# to strings:
#
# 0x7F1200A0 - string/abc_action_menu_overflow_description
#         (default) - [STR] "More options"
#         locale: "ca" - [STR] "Més opcions"
#         locale: "da" - [STR] "Flere muligheder"
#         locale: "fa" - [STR] " گزینه<U+200C>های بیشتر"
#         locale: "ja" - [STR] "その他のオプション"
#         locale: "ta" - [STR] "மேலும் விருப்பங்கள்"
#         locale: "nb" - [STR] "Flere alternativer"
#         ...
#
# Fun fact #1: Bundletool uses <lang>-<REGION> instead of <lang>-r<REGION>
#              for locales!
#
# Fun fact #2: The <U+200C> is terminal output for \u200c, the output is
#              really UTF-8 encoded when it is read by this script.
#
# Fun fact #3: Bundletool quotes \n, \\ and \" just like aapt since 0.8.0.
#
_RE_BUNDLE_STRING_RESOURCE_HEADER = re.compile(
    r'^0x([0-9A-F]+)\s\-\sstring/(\w+)$')
assert _RE_BUNDLE_STRING_RESOURCE_HEADER.match(
    '0x7F1200A0 - string/abc_action_menu_overflow_description')

_RE_BUNDLE_STRING_DEFAULT_VALUE = re.compile(
    r'^\s+\(default\) - \[STR\] "(.*)"$')
assert _RE_BUNDLE_STRING_DEFAULT_VALUE.match(
    '        (default) - [STR] "More options"')
assert _RE_BUNDLE_STRING_DEFAULT_VALUE.match(
    '        (default) - [STR] "More options"').group(1) == "More options"

_RE_BUNDLE_STRING_LOCALIZED_VALUE = re.compile(
    r'^\s+locale: "([0-9a-zA-Z-]+)" - \[STR\] "(.*)"$')
assert _RE_BUNDLE_STRING_LOCALIZED_VALUE.match(
    u'        locale: "ar" - [STR] "گزینه\u200cهای بیشتر"'.encode('utf-8'))


def ParseBundleResources(bundle_tool_jar_path, bundle_path):
  """Use bundletool to extract the localized strings of a given bundle.

  Args:
    bundle_tool_jar_path: Path to bundletool .jar executable.
    bundle_path: Path to input bundle.
  Returns:
    A new ResourceStringMap instance populated with the bundle's content.
  """
  cmd_args = [
      'java', '-jar', bundle_tool_jar_path, 'dump', 'resources', '--bundle',
      bundle_path, '--values'
  ]
  p = subprocess.Popen(cmd_args, bufsize=1, stdout=subprocess.PIPE)
  res_map = ResourceStringMap()
  current_resource_id = None
  current_resource_name = None
  keep_parsing = True
  need_value = False
  while keep_parsing:
    line = p.stdout.readline()
    if not line:
      break
    # Do not use rstrip(), since this should only remove trailing newlines
    # but not trailing whitespace that happen to be embedded in the string
    # value for some reason.
    line = line.rstrip('\n\r')
    m = _RE_BUNDLE_STRING_RESOURCE_HEADER.match(line)
    if m:
      current_resource_id = int(m.group(1), 16)
      current_resource_name = m.group(2)
      need_value = True
      continue

    if not need_value:
      continue

    resource_config = None
    m = _RE_BUNDLE_STRING_DEFAULT_VALUE.match(line)
    if m:
      resource_config = 'config (default)'
      resource_value = m.group(1)
    else:
      m = _RE_BUNDLE_STRING_LOCALIZED_VALUE.match(line)
      if m:
        resource_config = 'config %s' % m.group(1)
        resource_value = m.group(2)

    if resource_config is None:
      need_value = False
      continue

    res_map.AddValue(current_resource_id, current_resource_name,
                     resource_config, UnquoteString(resource_value))
  return res_map


# Name of the binary resources table file inside an APK.
RESOURCES_FILENAME = 'resources.arsc'


def IsFilePathAnApk(input_file):
  """Returns True iff a ZipFile instance is for a regular APK."""
  try:
    with zipfile.ZipFile(input_file) as input_zip:
      _ = input_zip.getinfo(RESOURCES_FILENAME)
      return True
  except:
    return False


# pylint: disable=line-too-long

# Example output from 'aapt dump resources --values' corresponding
# to strings:
#
#      config zh-rHK
#        resource 0x7f12009c org.chromium.chrome:string/0_resource_name_obfuscated: t=0x03 d=0x0000caa9 (s=0x0008 r=0x00)
#          (string8) "瀏覽首頁"
#        resource 0x7f12009d org.chromium.chrome:string/0_resource_name_obfuscated: t=0x03 d=0x0000c8e0 (s=0x0008 r=0x00)
#          (string8) "向上瀏覽"
#

# The following are compiled regular expressions used to recognize each
# of line and extract relevant information.
#
_RE_AAPT_CONFIG = re.compile(r'^\s+config (.+):$')
assert _RE_AAPT_CONFIG.match('   config (default):')
assert _RE_AAPT_CONFIG.match('   config zh-rTW:')

# Match an ISO 639-1 or ISO 639-2 locale.
_RE_AAPT_ISO_639_LOCALE = re.compile(r'^[a-z]{2,3}(-r[A-Z]{2,3})?$')
assert _RE_AAPT_ISO_639_LOCALE.match('de')
assert _RE_AAPT_ISO_639_LOCALE.match('zh-rTW')
assert _RE_AAPT_ISO_639_LOCALE.match('fil')
assert not _RE_AAPT_ISO_639_LOCALE.match('land')

_RE_AAPT_BCP47_LOCALE = re.compile(r'^b\+[a-z][a-zA-Z0-9\+]+$')
assert _RE_AAPT_BCP47_LOCALE.match('b+sr')
assert _RE_AAPT_BCP47_LOCALE.match('b+sr+Latn')
assert _RE_AAPT_BCP47_LOCALE.match('b+en+US')
assert not _RE_AAPT_BCP47_LOCALE.match('b+')
assert not _RE_AAPT_BCP47_LOCALE.match('b+1234')

_RE_AAPT_STRING_RESOURCE_HEADER = re.compile(
    r'^\s+resource 0x([0-9a-f]+) [a-zA-Z][a-zA-Z0-9.]+:string/(\w+):.*$')
assert _RE_AAPT_STRING_RESOURCE_HEADER.match(
    r'  resource 0x7f12009c org.chromium.chrome:string/0_resource_name_obfuscated: t=0x03 d=0x0000caa9 (s=0x0008 r=0x00)'
)

_RE_AAPT_STRING_RESOURCE_VALUE = re.compile(r'^\s+\(string8\) "(.*)"$')
assert _RE_AAPT_STRING_RESOURCE_VALUE.match(r'       (string8) "瀏覽首頁"')

# pylint: enable=line-too-long


def _ConvertAaptLocaleToBcp47(locale):
  """Convert a locale name from 'aapt dump' to its BCP-47 form."""
  if locale.startswith('b+'):
    return '-'.join(locale[2:].split('+'))
  lang, _, region = locale.partition('-r')
  if region:
    return '%s-%s' % (lang, region)
  return lang


assert _ConvertAaptLocaleToBcp47('(default)') == '(default)'
assert _ConvertAaptLocaleToBcp47('en') == 'en'
assert _ConvertAaptLocaleToBcp47('en-rUS') == 'en-US'
assert _ConvertAaptLocaleToBcp47('en-US') == 'en-US'
assert _ConvertAaptLocaleToBcp47('fil') == 'fil'
assert _ConvertAaptLocaleToBcp47('b+sr+Latn') == 'sr-Latn'


def ParseApkResources(aapt_path, apk_path):
  """Use aapt to extract the localized strings of a given bundle.

  Args:
    bundle_tool_jar_path: Path to bundletool .jar executable.
    bundle_path: Path to input bundle.
  Returns:
    A new ResourceStringMap instance populated with the bundle's content.
  """
  cmd_args = [aapt_path, 'dump', '--values', 'resources', apk_path]
  p = subprocess.Popen(cmd_args, bufsize=1, stdout=subprocess.PIPE)

  res_map = ResourceStringMap()
  current_locale = None
  current_resource_id = None
  current_resource_name = None
  need_value = False
  while True:
    line = p.stdout.readline().rstrip()
    if not line:
      break
    m = _RE_AAPT_CONFIG.match(line)
    if m:
      locale = None
      aapt_locale = m.group(1)
      if aapt_locale == '(default)':
        locale = aapt_locale
      elif _RE_AAPT_ISO_639_LOCALE.match(aapt_locale):
        locale = aapt_locale
      elif _RE_AAPT_BCP47_LOCALE.match(aapt_locale):
        locale = aapt_locale
      if locale is not None:
        current_locale = _ConvertAaptLocaleToBcp47(locale)
      continue

    if current_locale is None:
      continue

    if need_value:
      m = _RE_AAPT_STRING_RESOURCE_VALUE.match(line)
      if not m:
        # Should not happen
        sys.stderr.write('WARNING: Missing value for string ID 0x%08x "%s"' %
                         (current_resource_id, current_resource_name))
        resource_value = '<MISSING_STRING_%08x>' % current_resource_id
      else:
        resource_value = UnquoteString(m.group(1))

      res_map.AddValue(current_resource_id, current_resource_name,
                       'config %s' % current_locale, resource_value)
      need_value = False
    else:
      m = _RE_AAPT_STRING_RESOURCE_HEADER.match(line)
      if m:
        current_resource_id = int(m.group(1), 16)
        current_resource_name = m.group(2)
        need_value = True

  return res_map


def main(args):
  parser = argparse.ArgumentParser(
      description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
  parser.add_argument(
      'input_file',
      help='Input file path. This can be either an APK, or an app bundle.')
  parser.add_argument('--output', help='Optional output file path.')
  parser.add_argument(
      '--omit-ids',
      action='store_true',
      help='Omit resource IDs in the output. This is useful '
      'to compare the contents of two distinct builds of the '
      'same APK.')
  parser.add_argument(
      '--aapt-path',
      default=_AAPT_DEFAULT_PATH,
      help='Path to aapt executable. Optional for APKs.')
  parser.add_argument(
      '--r-txt-path',
      help='Path to an optional input R.txt file used to translate resource '
      'IDs to string names. Useful when resources names in the input files '
      'were obfuscated. NOTE: If ${INPUT_FILE}.R.txt exists, if will be used '
      'automatically by this script.')
  parser.add_argument(
      '--bundletool-path',
      default=_DEFAULT_BUNDLETOOL_PATH,
      help='Path to alternate bundletool .jar file. Only used for bundles.')
  parser.add_argument(
      '--profile', action='store_true', help='Enable Python profiling.')

  options = parser.parse_args(args)

  # Create a {res_id -> res_name} map for unobfuscation, if needed.
  res_id_name_map = {}
  r_txt_path = options.r_txt_path
  if not r_txt_path:
    candidate_r_txt_path = options.input_file + '.R.txt'
    if os.path.exists(candidate_r_txt_path):
      r_txt_path = candidate_r_txt_path

  if r_txt_path:
    res_id_name_map = ReadStringMapFromRTxt(r_txt_path)

  # Create a helper lambda that creates a new ResourceStringMap instance
  # based on the input file's type.
  if IsFilePathABundle(options.input_file):
    if not options.bundletool_path:
      parser.error(
          '--bundletool-path <BUNDLETOOL_JAR> is required to parse bundles.')

    # use bundletool to parse the bundle resources.
    def create_string_map():
      return ParseBundleResources(options.bundletool_path, options.input_file)

  elif IsFilePathAnApk(options.input_file):
    if not options.aapt_path:
      parser.error('--aapt-path <AAPT> is required to parse APKs.')

    # Use aapt dump to parse the APK resources.
    def create_string_map():
      return ParseApkResources(options.aapt_path, options.input_file)

  else:
    parser.error('Unknown file format: %s' % options.input_file)

  # Print everything now.
  with ManagedOutput(options.output) as output:
    with ManagedPythonProfiling(options.profile):
      res_map = create_string_map()
      res_map.RemapResourceNames(res_id_name_map)
      lines = AutoIndentStringList(res_map.ToStringList(options.omit_ids))
      for line in lines:
        output.write(line)
        output.write('\n')


if __name__ == "__main__":
  main(sys.argv[1:])
