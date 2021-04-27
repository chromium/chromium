#!/usr/bin/env python3
# coding: utf-8
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections
import os
import sys
import unittest

sys.path.insert(
    0, os.path.abspath(os.path.join(os.path.dirname(__file__), os.pardir)))
from util import build_utils

# Required because the following import needs build/android/gyp in the
# Python path to import util.build_utils.
_BUILD_ANDROID_GYP_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), os.pardir))
sys.path.insert(1, _BUILD_ANDROID_GYP_ROOT)

import resource_utils  # pylint: disable=relative-import

# pylint: disable=line-too-long

_TEST_XML_INPUT_1 = '''<?xml version="1.0" encoding="utf-8"?>
<resources xmlns:android="http://schemas.android.com/apk/res/android">
<string name="copy_to_clipboard_failure_message">"Lõikelauale kopeerimine ebaõnnestus"</string>
<string name="low_memory_error">"Eelmist toimingut ei saa vähese mälu tõttu lõpetada"</string>
<string name="opening_file_error">"Valit. faili avamine ebaõnnestus"</string>
<string name="structured_text">"This is <android:g id="STRUCTURED_TEXT">%s</android:g>"</string>
</resources>
'''

_TEST_XML_OUTPUT_2 = '''<?xml version="1.0" encoding="utf-8"?>
<resources xmlns:android="http://schemas.android.com/apk/res/android">
<string name="low_memory_error">"Eelmist toimingut ei saa vähese mälu tõttu lõpetada"</string>
<string name="structured_text">"This is <android:g id="STRUCTURED_TEXT">%s</android:g>"</string>
</resources>
'''

# pylint: enable=line-too-long

_TEST_XML_OUTPUT_EMPTY = '''<?xml version="1.0" encoding="utf-8"?>
<resources>
<!-- this file intentionally empty -->
</resources>
'''

_TEST_RESOURCES_MAP_1 = {
    'low_memory_error': 'Eelmist toimingut ei saa vähese mälu tõttu lõpetada',
    'opening_file_error': 'Valit. faili avamine ebaõnnestus',
    'copy_to_clipboard_failure_message': 'Lõikelauale kopeerimine ebaõnnestus',
    'structured_text': 'This is <android:g id="STRUCTURED_TEXT">%s</android:g>',
}

_TEST_NAMESPACES_1 = {'android': 'http://schemas.android.com/apk/res/android'}

_TEST_RESOURCES_ALLOWLIST_1 = ['low_memory_error', 'structured_text']

# Extracted from one generated Chromium R.txt file, with string resource
# names shuffled randomly.
_TEST_R_TXT = r'''int anim abc_fade_in 0x7f050000
int anim abc_fade_out 0x7f050001
int anim abc_grow_fade_in_from_bottom 0x7f050002
int array DefaultCookiesSettingEntries 0x7f120002
int array DefaultCookiesSettingValues 0x7f120003
int array DefaultGeolocationSettingEntries 0x7f120004
int attr actionBarDivider 0x7f0100e7
int attr actionBarStyle 0x7f0100e2
int string AllowedDomainsForAppsDesc 0x7f0c0105
int string AlternateErrorPagesEnabledDesc 0x7f0c0107
int string AuthAndroidNegotiateAccountTypeDesc 0x7f0c0109
int string AllowedDomainsForAppsTitle 0x7f0c0104
int string AlternateErrorPagesEnabledTitle 0x7f0c0106
int[] styleable SnackbarLayout { 0x0101011f, 0x7f010076, 0x7f0100ba }
int styleable SnackbarLayout_android_maxWidth 0
int styleable SnackbarLayout_elevation 2
'''

# Test allowlist R.txt file. Note that AlternateErrorPagesEnabledTitle is
# listed as an 'anim' and should thus be skipped. Similarly the string
# 'ThisStringDoesNotAppear' should not be in the final result.
_TEST_ALLOWLIST_R_TXT = r'''int anim AlternateErrorPagesEnabledTitle 0x7f0eeeee
int string AllowedDomainsForAppsDesc 0x7f0c0105
int string AlternateErrorPagesEnabledDesc 0x7f0c0107
int string ThisStringDoesNotAppear 0x7f0fffff
'''

_TEST_R_TEXT_RESOURCES_IDS = {
    0x7f0c0105: 'AllowedDomainsForAppsDesc',
    0x7f0c0107: 'AlternateErrorPagesEnabledDesc',
}

# Names of string resources in _TEST_R_TXT, should be sorted!
_TEST_R_TXT_STRING_RESOURCE_NAMES = sorted([
    'AllowedDomainsForAppsDesc',
    'AllowedDomainsForAppsTitle',
    'AlternateErrorPagesEnabledDesc',
    'AlternateErrorPagesEnabledTitle',
    'AuthAndroidNegotiateAccountTypeDesc',
])


def _CreateTestFile(tmp_dir, file_name, file_data):
  file_path = os.path.join(tmp_dir, file_name)
  with open(file_path, 'wt') as f:
    f.write(file_data)
  return file_path



class ResourceUtilsTest(unittest.TestCase):

  def test_GetRTxtStringResourceNames(self):
    with build_utils.TempDir() as tmp_dir:
      tmp_file = _CreateTestFile(tmp_dir, "test_R.txt", _TEST_R_TXT)
      self.assertListEqual(
          resource_utils.GetRTxtStringResourceNames(tmp_file),
          _TEST_R_TXT_STRING_RESOURCE_NAMES)

  def test_GenerateStringResourcesAllowList(self):
    with build_utils.TempDir() as tmp_dir:
      tmp_module_rtxt_file = _CreateTestFile(tmp_dir, "test_R.txt", _TEST_R_TXT)
      tmp_allowlist_rtxt_file = _CreateTestFile(tmp_dir, "test_allowlist_R.txt",
                                                _TEST_ALLOWLIST_R_TXT)
      self.assertDictEqual(
          resource_utils.GenerateStringResourcesAllowList(
              tmp_module_rtxt_file, tmp_allowlist_rtxt_file),
          _TEST_R_TEXT_RESOURCES_IDS)

  def test_IsAndroidLocaleQualifier(self):
    good_locales = [
        'en',
        'en-rUS',
        'fil',
        'fil-rPH',
        'iw',
        'iw-rIL',
        'b+en',
        'b+en+US',
        'b+ja+Latn',
        'b+ja+JP+Latn',
        'b+cmn+Hant-TW',
    ]
    bad_locales = [
        'e', 'english', 'en-US', 'en_US', 'en-rus', 'b+e', 'b+english', 'b+ja+'
    ]
    for locale in good_locales:
      self.assertTrue(
          resource_utils.IsAndroidLocaleQualifier(locale),
          msg="'%s' should be a good locale!" % locale)

    for locale in bad_locales:
      self.assertFalse(
          resource_utils.IsAndroidLocaleQualifier(locale),
          msg="'%s' should be a bad locale!" % locale)

  def test_ToAndroidLocaleName(self):
    _TEST_CHROMIUM_TO_ANDROID_LOCALE_MAP = {
        'en': 'en',
        'en-US': 'en-rUS',
        'en-FOO': 'en-rFOO',
        'fil': 'tl',
        'tl': 'tl',
        'he': 'iw',
        'he-IL': 'iw-rIL',
        'id': 'in',
        'id-BAR': 'in-rBAR',
        'nb': 'nb',
        'yi': 'ji'
    }
    for chromium_locale, android_locale in \
        _TEST_CHROMIUM_TO_ANDROID_LOCALE_MAP.items():
      result = resource_utils.ToAndroidLocaleName(chromium_locale)
      self.assertEqual(result, android_locale)

  def test_ToChromiumLocaleName(self):
    _TEST_ANDROID_TO_CHROMIUM_LOCALE_MAP = {
        'foo': 'foo',
        'foo-rBAR': 'foo-BAR',
        'b+lll': 'lll',
        'b+ll+Extra': 'll',
        'b+ll+RR': 'll-RR',
        'b+lll+RR+Extra': 'lll-RR',
        'b+ll+RRR+Extra': 'll-RRR',
        'b+ll+Ssss': 'll-Ssss',
        'b+ll+Ssss+Extra': 'll-Ssss',
        'b+ll+Ssss+RR': 'll-Ssss-RR',
        'b+ll+Ssss+RRR': 'll-Ssss-RRR',
        'b+ll+Ssss+RRR+Extra': 'll-Ssss-RRR',
        'b+ll+Whatever': 'll',
        'en': 'en',
        'en-rUS': 'en-US',
        'en-US': None,
        'en-FOO': None,
        'en-rFOO': 'en-FOO',
        'es-rES': 'es-ES',
        'es-rUS': 'es-419',
        'tl': 'fil',
        'fil': 'fil',
        'iw': 'he',
        'iw-rIL': 'he-IL',
        'b+iw+IL': 'he-IL',
        'in': 'id',
        'in-rBAR': 'id-BAR',
        'id-rBAR': 'id-BAR',
        'nb': 'nb',
        'no': 'nb',  # http://crbug.com/920960
    }
    for android_locale, chromium_locale in \
        _TEST_ANDROID_TO_CHROMIUM_LOCALE_MAP.items():
      result = resource_utils.ToChromiumLocaleName(android_locale)
      self.assertEqual(result, chromium_locale)

  def test_FindLocaleInStringResourceFilePath(self):
    self.assertEqual(
        None,
        resource_utils.FindLocaleInStringResourceFilePath(
            'res/values/whatever.xml'))
    self.assertEqual(
        'foo',
        resource_utils.FindLocaleInStringResourceFilePath(
            'res/values-foo/whatever.xml'))
    self.assertEqual(
        'foo-rBAR',
        resource_utils.FindLocaleInStringResourceFilePath(
            'res/values-foo-rBAR/whatever.xml'))
    self.assertEqual(
        None,
        resource_utils.FindLocaleInStringResourceFilePath(
            'res/values-foo/ignore-subdirs/whatever.xml'))

  def test_ParseAndroidResourceStringsFromXml(self):
    ret, namespaces = resource_utils.ParseAndroidResourceStringsFromXml(
        _TEST_XML_INPUT_1)
    self.assertDictEqual(ret, _TEST_RESOURCES_MAP_1)
    self.assertDictEqual(namespaces, _TEST_NAMESPACES_1)

  def test_GenerateAndroidResourceStringsXml(self):
    # Fist, an empty strings map, with no namespaces
    result = resource_utils.GenerateAndroidResourceStringsXml({})
    self.assertEqual(result.decode('utf8'), _TEST_XML_OUTPUT_EMPTY)

    result = resource_utils.GenerateAndroidResourceStringsXml(
        _TEST_RESOURCES_MAP_1, _TEST_NAMESPACES_1)
    self.assertEqual(result.decode('utf8'), _TEST_XML_INPUT_1)

  @staticmethod
  def _CreateTestResourceFile(output_dir, locale, string_map, namespaces):
    values_dir = os.path.join(output_dir, 'values-' + locale)
    build_utils.MakeDirectory(values_dir)
    file_path = os.path.join(values_dir, 'strings.xml')
    with open(file_path, 'wb') as f:
      file_data = resource_utils.GenerateAndroidResourceStringsXml(
          string_map, namespaces)
      f.write(file_data)
    return file_path

  def _CheckTestResourceFile(self, file_path, expected_data):
    with open(file_path) as f:
      file_data = f.read()
    self.assertEqual(file_data, expected_data)

  def test_FilterAndroidResourceStringsXml(self):
    with build_utils.TempDir() as tmp_path:
      test_file = self._CreateTestResourceFile(
          tmp_path, 'foo', _TEST_RESOURCES_MAP_1, _TEST_NAMESPACES_1)
      resource_utils.FilterAndroidResourceStringsXml(
          test_file, lambda x: x in _TEST_RESOURCES_ALLOWLIST_1)
      self._CheckTestResourceFile(test_file, _TEST_XML_OUTPUT_2)


if __name__ == '__main__':
  unittest.main()
