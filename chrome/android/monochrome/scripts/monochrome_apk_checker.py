#!/usr/bin/env python2.7
#
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import re
import os
import posixpath
import StringIO
import sys
import subprocess

from contextlib import closing


def BuildFileMatchRegex(*file_matchers):
  return re.compile('^' + '|'.join(file_matchers) + '$')


# Chrome specific files which are not in Monochrome.apk
CHROME_SPECIFIC = BuildFileMatchRegex(
    r'lib/.*/libchrome\.so',
    r'lib/.*/libchrome\.\d{4}\.\d{2,3}\.so', # libchrome placeholders
    r'lib/.*/libchromium_android_linker\.so',
    r'lib/.*/libchromeview\.so', # placeholder library
    r'lib/.*/libchrome_crashpad_handler\.so',
    r'lib/.*/crazy\.libchrome\.so',
    r'lib/.*/crazy\.libchrome\.align',
    r'lib/.*/gdbserver',
     # Monochrome doesn't have any res directories whose api number is less
     # than v24.
    r'res/.*-v1\d/.*\.xml',
    r'res/.*-v2[0-3]/.*\.xml',
    r'META-INF/.*',
    r'assets/metaresources.arsc',
    r'assets/AndroidManifest.xml')

# WebView specific files which are not in Monochrome.apk
WEBVIEW_SPECIFIC = BuildFileMatchRegex(
    r'lib/.*/libwebviewchromium\.so',
    r'assets/webview_licenses.notice',
    r'res/.*/icon_webview(.webp)?',
    r'META-INF/.*',
     # Monochrome doesn't have any res directories
     # whose api level is less than v24.
    r'res/.*-v1\d/.*\.xml',
    r'res/.*-v2[0-3]/.*\.xml',
    r'lib/.*/gdbserver')

# The files in Chrome are not same as those in Monochrome
CHROME_CHANGES = BuildFileMatchRegex(
    r'AndroidManifest\.xml',
    r'resources\.arsc',
    r'classes\.dex',
    r'classes2\.dex',
    r'res/.*\.xml', # Resource id isn't same
    r'assets/unwind_cfi_32', # Generated from apk's shared library
     # All pak files except chrome_100_percent.pak are different
    r'assets/resources\.pak',
    r'assets/locales/am\.pak',
    r'assets/locales/ar\.pak',
    r'assets/locales/bg\.pak',
    r'assets/locales/ca\.pak',
    r'assets/locales/cs\.pak',
    r'assets/locales/da\.pak',
    r'assets/locales/de\.pak',
    r'assets/locales/el\.pak',
    r'assets/locales/en-GB\.pak',
    r'assets/locales/en-US\.pak',
    r'assets/locales/es-419\.pak',
    r'assets/locales/es\.pak',
    r'assets/locales/fa\.pak',
    r'assets/locales/fi\.pak',
    r'assets/locales/fil\.pak',
    r'assets/locales/fr\.pak',
    r'assets/locales/he\.pak',
    r'assets/locales/hi\.pak',
    r'assets/locales/hr\.pak',
    r'assets/locales/hu\.pak',
    r'assets/locales/id\.pak',
    r'assets/locales/it\.pak',
    r'assets/locales/ja\.pak',
    r'assets/locales/ko\.pak',
    r'assets/locales/lt\.pak',
    r'assets/locales/lv\.pak',
    r'assets/locales/nb\.pak',
    r'assets/locales/nl\.pak',
    r'assets/locales/pl\.pak',
    r'assets/locales/pt-BR\.pak',
    r'assets/locales/pt-PT\.pak',
    r'assets/locales/ro\.pak',
    r'assets/locales/ru\.pak',
    r'assets/locales/sk\.pak',
    r'assets/locales/sl\.pak',
    r'assets/locales/sr\.pak',
    r'assets/locales/sv\.pak',
    r'assets/locales/sw\.pak',
    r'assets/locales/th\.pak',
    r'assets/locales/tr\.pak',
    r'assets/locales/uk\.pak',
    r'assets/locales/vi\.pak',
    r'assets/locales/zh-CN\.pak',
    r'assets/locales/zh-TW\.pak')

# The files in WebView are not same as those in Monochrome
WEBVIEW_CHANGES = BuildFileMatchRegex(
    r'AndroidManifest\.xml',
    r'resources\.arsc',
    r'classes\.dex',
    r'res/.*\.xml', # Resource id isn't same
    r'assets/.*\.pak') # All pak files are not same as Monochrome

# Parse the output of unzip -lv, like
# 2384  Defl:N      807  66% 2001-01-01 00:00 2f2d9fce  res/xml/privacy.xml
ZIP_ENTRY = re.compile(
    "^ *[0-9]+ +\S+ +[0-9]+ +(?P<cmpr>[0-9]{1,2})% +\S+ +\S+ +"
    "(?P<crc>[0-9a-fA-F]+) +(?P<name>\S+)"
  )

class APKEntry:
  def __init__(self,  filename, crc, uncompressed):
    self.filename = filename
    self.CRC = crc
    self.uncompressed = uncompressed

def DumpAPK(apk):
  args = ['unzip', '-lv']
  args.append(apk)
  content = subprocess.check_output(args)
  apk_entries = []
  with closing(StringIO.StringIO(content)) as f:
    for line in f:
      match = ZIP_ENTRY.match(line)
      if match:
        apk_entries.append(
            APKEntry(
                match.group('name'), match.group('crc'),
                match.group('cmpr') == 0))
  return apk_entries

def VerifySameFile(monochrome_dict, apk, changes):
  """Verify apk file content matches same files in monochrome.

  Verify files from apk are same as those in monochrome except files
  in changes.
  """
  diff = []
  for a in apk:
    # File may not exists due to exists_in_some_form().
    m = monochrome_dict.get(a.filename)
    if m and m.CRC != a.CRC and not changes.match(m.filename):
      diff.append(a.filename)
  if len(diff):
    raise Exception("The following files are not same as Monochrome:\n %s" %
                    '\n'.join(diff))


def VerifyUncompressed(monochrome, apk):
  """Verify uncompressed files in apk are a subset of those in monochrome.

  Verify files not being compressed in apk are also uncompressed in
  Monochrome APK.
  """
  uncompressed = [i.filename for i in apk if i.uncompressed ]
  monochrome_uncompressed = [i.filename for i in monochrome if i.uncompressed]
  compressed = [u for u in uncompressed if u not in monochrome_uncompressed]
  if len(compressed):
    raise Exception("The following files are compressed in Monochrome:\n %s" %
                    '\n'.join(compressed))

def SuperSetOf(monochrome, apk):
  """Verify Monochrome is super set of apk."""

  def exists_in_some_form(f):
    if f in monochrome:
      return True
    if not f.startswith('res/'):
      return False
    name = '/' + posixpath.basename(f)
    # Some resources will exists in apk but not in monochrome due to the
    # difference in minSdkVersion. https://crbug.com/794438
    # E.g.:
    # apk could have: res/drawable/foo.png, res/drawable-v23/foo.png
    # monochrome (minSdkVersion=24) would need only: res/drawable-v23/foo.png
    return any(x.endswith(name) for x in monochrome)

  missing_files = [f for f in apk if not exists_in_some_form(f)]
  if len(missing_files):
    raise Exception('The following files are missing in Monochrome:\n %s' %
                    '\n'.join(missing_files))


def RemoveSpecific(apk_entries, specific):
  return [i for i in apk_entries
          if not specific.search(i.filename) ]


def LoadPathmap(pathmap_path):
  """Load the pathmap of obfuscated resource paths.

  Returns: A dict mapping from obfuscated paths to original paths or an
           empty dict if passed a None |pathmap_path|.
  """
  if pathmap_path is None:
    return {}

  pathmap = {}
  with open(pathmap_path, 'r') as f:
    for line in f:
      line = line.strip()
      if line.startswith('#') or line == '':
        continue
      original, renamed = line.split(' -> ')
      pathmap[renamed] = original
  return pathmap


def DeobfuscateFilename(obfuscated_filename, pathmap):
  return pathmap.get(obfuscated_filename, obfuscated_filename)


def ParseArgs(args):
  """Parses command line options.

  Returns:
    An Namespace from argparse.parse_args()
  """
  parser = argparse.ArgumentParser(prog='monochrome_apk_checker')

  parser.add_argument(
      '--monochrome-apk', required=True, help='The monochrome APK path.')
  parser.add_argument(
      '--monochrome-pathmap', help='The monochrome APK resources pathmap path.')
  parser.add_argument('--chrome-apk',
                      required=True,
                      help='The chrome APK path.')
  parser.add_argument(
      '--chrome-pathmap', help='The chrome APK resources pathmap path.')
  parser.add_argument('--system-webview-apk',
                      required=True,
                      help='The system webview APK path.')
  parser.add_argument(
      '--system-webview-pathmap',
      help='The system webview APK resources pathmap path.')
  return parser.parse_args(args)


def main():
  options = ParseArgs(sys.argv[1:])
  monochrome = DumpAPK(options.monochrome_apk)
  monochrome_pathmap = LoadPathmap(options.monochrome_pathmap)
  monochrome_files = [
      DeobfuscateFilename(f.filename, monochrome_pathmap) for f in monochrome
  ]
  monochrome_dict = dict([(DeobfuscateFilename(i.filename, monochrome_pathmap),
                           i) for i in monochrome])

  chrome = RemoveSpecific(DumpAPK(options.chrome_apk),
                          CHROME_SPECIFIC)
  if len(chrome) == 0:
    raise Exception('Chrome should have common files with Monochrome')

  webview = RemoveSpecific(DumpAPK(options.system_webview_apk),
                           WEBVIEW_SPECIFIC)
  if len(webview) == 0:
    raise Exception('WebView should have common files with Monochrome')

  def check_apk(apk, pathmap):
    apk_files = [DeobfuscateFilename(f.filename, pathmap) for f in apk]
    SuperSetOf(monochrome_files, apk_files)
    VerifyUncompressed(monochrome, apk)
    VerifySameFile(monochrome_dict, chrome, CHROME_CHANGES)
    VerifySameFile(monochrome_dict, webview, WEBVIEW_CHANGES)

  chrome_pathmap = LoadPathmap(options.chrome_pathmap)
  check_apk(chrome, chrome_pathmap)

  webview_pathmap = LoadPathmap(options.system_webview_pathmap)
  check_apk(webview, webview_pathmap)

if __name__ == '__main__':
  sys.exit(main())
