#!/usr/bin/env python3
#
# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generate a zip archive containing localized locale name Android resource
strings!

This script takes a list of input Chrome-specific locale names, as well as an
output zip file path.

Each output file will contain the definition of a single string resource,
named 'current_locale', whose value will be the matching Chromium locale name.
E.g. values-en-rUS/strings.xml will define 'current_locale' as 'en-US'.
"""

import argparse
import os
import sys
import zipfile

sys.path.insert(
    0,
    os.path.join(
        os.path.dirname(__file__), '..', '..', '..', 'build', 'android', 'gyp'))

from util import build_utils
from util import resource_utils
import action_helpers  # build_utils adds //build to sys.path.
import zip_helpers


# A small string template for the content of each strings.xml file.
# NOTE: The name is chosen to avoid any conflicts with other string defined
# by other resource archives.
_TEMPLATE = """\
<?xml version="1.0" encoding="utf-8"?>
<resources>
    <string name="current_detected_ui_locale_name">{resource_text}</string>
</resources>
"""

# The default Chrome locale value.
_DEFAULT_CHROME_LOCALE = 'en-US'


def _GenerateLocaleStringsXml(locale):
  return _TEMPLATE.format(resource_text=locale)


def _AddLocaleResourceFileToZip(out_zip, android_locale, locale):
  locale_data = _GenerateLocaleStringsXml(locale)
  if android_locale:
    zip_path = 'values-%s/strings.xml' % android_locale
  else:
    zip_path = 'values/strings.xml'
  zip_helpers.add_to_zip_hermetic(out_zip,
                                  zip_path,
                                  data=locale_data,
                                  compress=False)


def main():
  parser = argparse.ArgumentParser(
      description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)

  parser.add_argument(
      '--locale-list',
      required=True,
      help='GN-list of Chrome-specific locale names.')
  parser.add_argument(
      '--output-zip', required=True, help='Output zip archive path.')

  args = parser.parse_args()

  locale_list = action_helpers.parse_gn_list(args.locale_list)
  if not locale_list:
    raise Exception('Locale list cannot be empty!')

  with action_helpers.atomic_output(args.output_zip) as tmp_file:
    with zipfile.ZipFile(tmp_file, 'w') as out_zip:
      # First, write the default value, since aapt requires one.
      _AddLocaleResourceFileToZip(out_zip, '', _DEFAULT_CHROME_LOCALE)

      for locale in locale_list:
        android_locale = resource_utils.ToAndroidLocaleName(locale)
        _AddLocaleResourceFileToZip(out_zip, android_locale, locale)


if __name__ == '__main__':
  main()
