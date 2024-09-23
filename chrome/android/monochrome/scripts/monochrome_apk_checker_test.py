# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import contextlib
import io
import os
import posixpath
import re
import subprocess

import typ

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
    # R.font.accent_font is an alias in internal Chrome.
    r'res/.*/accent_font.xml',
    # Monochrome doesn't have any res directories whose api number is less
    # than v24.
    r'res/.*-v1\d/.*\.xml',
    r'res/.*-v2[0-3]/.*\.xml',
    r'META-INF/.*',
    r'assets/dexopt/baseline.prof',
    r'assets/dexopt/baseline.profm',
    r'assets/metaresources.arsc',
    r'assets/AndroidManifest.xml')

# WebView specific files which are not in Monochrome.apk
WEBVIEW_SPECIFIC = BuildFileMatchRegex(
    r'lib/.*/libwebviewchromium\.so',
    r'lib/.*/libchromium_android_linker\.so',
    r'assets/webview_licenses.notice',
    r'res/.*/accent_font.xml',
    r'res/.*/icon_webview(.webp)?',
    r'META-INF/.*',
     # Monochrome doesn't have any res directories
     # whose api level is less than v24.
    r'res/.*-v1\d/.*\.xml',
    r'res/.*-v2[0-3]/.*\.xml',
    r'lib/.*/gdbserver',
    # libarcore is only added to the aab version of monochrome.
    r'lib/.*/libarcore_sdk_c\.so')

# The files in Chrome are not same as those in Monochrome
CHROME_CHANGES = BuildFileMatchRegex(
    r'AndroidManifest\.xml',
    r'resources\.arsc',
    r'classes\d*\.dex',
    r'res/.*\.xml', # Resource id isn't same
    r'assets/unwind_cfi_32', # Generated from apk's shared library
     # All pak files except chrome_100_percent.pak are different
    r'assets/resources\.pak',
    r'assets/locales/.*\.pak')

# The files in WebView are not same as those in Monochrome
WEBVIEW_CHANGES = BuildFileMatchRegex(
    r'AndroidManifest\.xml',
    r'resources\.arsc',
    r'classes\d?\.dex',
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
  content = subprocess.check_output(args, universal_newlines=True)
  apk_entries = []
  with contextlib.closing(io.StringIO(content)) as f:
    for line in f:
      match = ZIP_ENTRY.match(line)
      if match:
        apk_entries.append(
            APKEntry(
                match.group('name'), match.group('crc'),
                match.group('cmpr') == 0))
  return apk_entries


def DeobfuscateFilename(obfuscated_filename, pathmap):
  path = pathmap.get(obfuscated_filename, obfuscated_filename)
  # Undo asset path prefixing. https://crbug.com/357131361
  if path.endswith('+'):
    suffix_idx = path.rfind('+', 0, len(path) - 1)
    if suffix_idx != -1:
      path = path[:suffix_idx]
  return path


class MonochromeApkCheckerTest(typ.TestCase):
  def VerifySameFile(self, monochrome_dict, apk, changes, apk_name):
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
    self.assertEquals(len(diff), 0,  """\
Unless specifcially excepted, all files in {0} should be exactly the same as
the similarly named file in Monochrome. However these files were present in
both monochrome and {0}, but had different contents:
{1}
""".format(apk_name, '\n'.join(diff)))


  def VerifyUncompressed(self, monochrome, apk, apk_name):
    """Verify uncompressed files in apk are a subset of those in monochrome.

    Verify files not being compressed in apk are also uncompressed in
    Monochrome APK.
    """
    uncompressed = [i.filename for i in apk if i.uncompressed ]
    monochrome_uncompressed = [i.filename for i in monochrome if i.uncompressed]
    compressed = [u for u in uncompressed if u not in monochrome_uncompressed]
    self.assertEquals(len(compressed), 0, """\
Uncompressed files in {0} should also be uncompressed in Monochrome.
However these files were uncompressed in {0} but compressed in Monochrome:
{1}
""".format(apk_name, '\n'.join(compressed)))


  def SuperSetOf(self, monochrome, apk, apk_name):
    """Verify Monochrome is super set of apk."""

    def exists_in_some_form(f):
      if f in monochrome:
        return True
      # Chrome.apk may have an extra classes.dex due to jdk library desugaring.
      if f.startswith('classes') and f.endswith('.dex'):
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
    self.assertEquals(len(missing_files), 0, """\
Monochrome is expected to have a superset of the files in {0}.
However these files were present in {0} but not in Monochrome:
{1}
""".format(apk_name, '\n'.join(missing_files)))


  def RemoveSpecific(self, apk_entries, specific):
    return [i for i in apk_entries
            if not specific.search(i.filename) ]


  def LoadPathmap(self, pathmap_path):
    """Load the pathmap of obfuscated resource paths.

    Returns: A dict mapping from obfuscated paths to original paths or an
           empty dict if passed a None |pathmap_path|.
    """
    if pathmap_path is None or not os.path.exists(pathmap_path):
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


  def testApkChecker(self):
    options = self.context

    monochrome = DumpAPK(options.monochrome_apk)
    monochrome_pathmap = self.LoadPathmap(options.monochrome_pathmap)
    monochrome_files = [
        DeobfuscateFilename(f.filename, monochrome_pathmap)
          for f in monochrome
    ]
    monochrome_dict = dict((DeobfuscateFilename(i.filename, monochrome_pathmap),
                            i) for i in monochrome)

    chrome = self.RemoveSpecific(DumpAPK(options.chrome_apk),
                                 CHROME_SPECIFIC)
    self.assertTrue(len(chrome) > 0,
        'Chrome should have common files with Monochrome. However the passed '
        'in APKs do not have any files in common. Are you sure you are passing '
        'in the right arguments?')

    webview = self.RemoveSpecific(DumpAPK(options.system_webview_apk),
                                  WEBVIEW_SPECIFIC)
    self.assertTrue(len(webview) > 0,
        'Webview should have common files with Monochrome. However the passed '
        'in APKs do not have any files in common. Are you sure you are passing '
        'in the right arguments?')

    def check_apk(apk, pathmap, apk_name):
      apk_files = [DeobfuscateFilename(f.filename, pathmap) for f in apk]
      self.SuperSetOf(monochrome_files, apk_files, apk_name)
      self.VerifyUncompressed(monochrome, apk, apk_name)

    chrome_pathmap = self.LoadPathmap(options.chrome_pathmap)
    check_apk(chrome, chrome_pathmap, 'Chrome')
    self.VerifySameFile(monochrome_dict, chrome, CHROME_CHANGES, 'Chrome')

    webview_pathmap = self.LoadPathmap(options.system_webview_pathmap)
    check_apk(webview, webview_pathmap, 'Webview')
    self.VerifySameFile(monochrome_dict, webview, WEBVIEW_CHANGES, 'Webview')
