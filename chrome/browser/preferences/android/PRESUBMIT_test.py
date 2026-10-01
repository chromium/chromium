#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os.path
import sys
import unittest

import PRESUBMIT

file_dir_path = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(file_dir_path, '..', '..', '..', '..'))
from PRESUBMIT_test_mocks import MockAffectedFile
from PRESUBMIT_test_mocks import MockInputApi, MockOutputApi


class CheckNoNewSharedPreferencesCountersTest(unittest.TestCase):
  def _create_mock_file(
    self,
    old_declarations,
    new_declarations,
    file_path=PRESUBMIT.CHROME_PREFERENCE_KEYS_PATH,
  ):
    old_lines = [
      'package org.chromium.chrome.browser.preferences;',
      'public final class ChromePreferenceKeys {',
    ]
    old_lines.extend(old_declarations)
    old_lines.append('}')

    new_lines = [
      'package org.chromium.chrome.browser.preferences;',
      'public final class ChromePreferenceKeys {',
    ]
    new_lines.extend(new_declarations)
    new_lines.append('}')

    mock_file = MockAffectedFile(file_path, new_lines, old_lines)
    return mock_file

  def testNoFilesModifiedPasses(self):
    mock_input_api = MockInputApi()
    mock_input_api.files = []
    results = PRESUBMIT.CheckNoNewSharedPreferencesCounters(
      mock_input_api, MockOutputApi()
    )
    self.assertEqual([], results)

  def testUnrelatedFileModifiedPasses(self):
    mock_input_api = MockInputApi()
    mock_input_api.files = [
      MockAffectedFile('some/other/File.java', ['public class File {}'])
    ]
    results = PRESUBMIT.CheckNoNewSharedPreferencesCounters(
      mock_input_api, MockOutputApi()
    )
    self.assertEqual([], results)

  def testNoDiffsPasses(self):
    old_decls = []
    new_decls = [
      '    public static final String MY_FEATURE_VIEW_COUNT = '
      '"Chrome.MyFeature.ViewCount";',
    ]
    mock_file = self._create_mock_file(old_decls, new_decls)
    mock_input_api = MockInputApi()
    mock_input_api.files = [mock_file]
    mock_input_api.no_diffs = True
    results = PRESUBMIT.CheckNoNewSharedPreferencesCounters(
      mock_input_api, MockOutputApi()
    )
    self.assertEqual([], results)

  def testAllowedNewKeyPasses(self):
    old_decls = [
      '    public static final String EXISTING_KEY = "existing_key";',
    ]
    new_decls = [
      '    public static final String EXISTING_KEY = "existing_key";',
      '    public static final String MY_FEATURE_ENABLED = '
      '"Chrome.MyFeature.Enabled";',
      '    public static final String LAST_SELECTED_ACCOUNT = '
      '"last_selected_account";',
    ]
    mock_file = self._create_mock_file(old_decls, new_decls)
    mock_input_api = MockInputApi()
    mock_input_api.files = [mock_file]
    results = PRESUBMIT.CheckNoNewSharedPreferencesCounters(
      mock_input_api, MockOutputApi()
    )
    self.assertEqual([], results)

  def testExistingKeyUnchangedPasses(self):
    old_decls = [
      '    public static final String READER_MODE_ACTION_SHOW_COUNT = '
      '"Chrome.ReaderMode.ActionShowCount";',
      '    public static final String '
      'IMAGE_DESCRIPTIONS_JUST_ONCE_COUNT = '
      '"image_descriptions_just_once_count";',
    ]
    new_decls = [
      '    public static final String READER_MODE_ACTION_SHOW_COUNT = '
      '"Chrome.ReaderMode.ActionShowCount";',
      '    public static final String '
      'IMAGE_DESCRIPTIONS_JUST_ONCE_COUNT = '
      '"image_descriptions_just_once_count";',
      '    public static final String NEW_NORMAL_KEY = '
      '"Chrome.MyFeature.NormalKey";',
    ]
    mock_file = self._create_mock_file(old_decls, new_decls)
    mock_input_api = MockInputApi()
    mock_input_api.files = [mock_file]
    results = PRESUBMIT.CheckNoNewSharedPreferencesCounters(
      mock_input_api, MockOutputApi()
    )
    self.assertEqual([], results)

  def testNewCountKeyWarns(self):
    old_decls = []
    new_decls = [
      '    public static final String MY_FEATURE_VIEW_COUNT = '
      '"Chrome.MyFeature.ViewCount";',
      '    public static final String '
      'HOME_MODULES_IMPRESSION_COUNT_BEFORE_INTERACTION = '
      '"Chrome.HomeModules.ImpressionCountBeforeInteraction";',
      '    public static final String MY_FEATURE_COUNT_KEY = '
      '"Chrome.MyFeature.CountKey";',
    ]
    mock_file = self._create_mock_file(old_decls, new_decls)
    mock_input_api = MockInputApi()
    mock_input_api.files = [mock_file]
    results = PRESUBMIT.CheckNoNewSharedPreferencesCounters(
      mock_input_api, MockOutputApi()
    )
    self.assertEqual(1, len(results))
    self.assertEqual('warning', results[0].type)
    self.assertIn('MY_FEATURE_VIEW_COUNT', results[0].message)
    self.assertIn(
      'HOME_MODULES_IMPRESSION_COUNT_BEFORE_INTERACTION', results[0].message
    )
    self.assertIn('MY_FEATURE_COUNT_KEY', results[0].message)
    self.assertIn('feature engagement tracker', results[0].message)

  def testNewCounterKeyWarns(self):
    old_decls = []
    new_decls = [
      '    public static final String PROMO_CLICK_COUNTER = '
      '"Chrome.Promo.ClickCounter";',
    ]
    mock_file = self._create_mock_file(old_decls, new_decls)
    mock_input_api = MockInputApi()
    mock_input_api.files = [mock_file]
    results = PRESUBMIT.CheckNoNewSharedPreferencesCounters(
      mock_input_api, MockOutputApi()
    )
    self.assertEqual(1, len(results))
    self.assertEqual('warning', results[0].type)
    self.assertIn('PROMO_CLICK_COUNTER', results[0].message)

  def testNewNumberKeyWarns(self):
    old_decls = []
    new_decls = [
      '    public static final String NUMBER_OF_DISMISSALS = '
      '"number_of_dismissals";',
      '    public static final String NUM_TIMES_OPENED = '
      '"Chrome.Feature.NumTimesOpened";',
    ]
    mock_file = self._create_mock_file(old_decls, new_decls)
    mock_input_api = MockInputApi()
    mock_input_api.files = [mock_file]
    results = PRESUBMIT.CheckNoNewSharedPreferencesCounters(
      mock_input_api, MockOutputApi()
    )
    self.assertEqual(1, len(results))
    self.assertEqual('warning', results[0].type)
    self.assertIn('NUMBER_OF_DISMISSALS', results[0].message)
    self.assertIn('NUM_TIMES_OPENED', results[0].message)

  def testNewActionInteractionKeyWarns(self):
    old_decls = []
    new_decls = [
      '    public static final String CONTEXT_MENU_ITEM_CLICKED = '
      '"Chrome.ContextMenu.ItemClicked";',
      '    public static final String OPT_IN_PROMO_DISMISSED = '
      '"opt_in_promo_dismissed";',
    ]
    mock_file = self._create_mock_file(old_decls, new_decls)
    mock_input_api = MockInputApi()
    mock_input_api.files = [mock_file]
    results = PRESUBMIT.CheckNoNewSharedPreferencesCounters(
      mock_input_api, MockOutputApi()
    )
    self.assertEqual(1, len(results))
    self.assertEqual('warning', results[0].type)
    self.assertIn('CONTEXT_MENU_ITEM_CLICKED', results[0].message)
    self.assertIn('OPT_IN_PROMO_DISMISSED', results[0].message)

  def testNewImpressionKeyWarns(self):
    old_decls = []
    new_decls = [
      '    public static final String NEW_TAB_PAGE_PROMO_SHOWN = '
      '"Chrome.NTP.PromoShown";',
      '    public static final String BOTTOM_SHEET_SEEN = "bottom_sheet_seen";',
      '    public static final String PROMO_IMPRESSION = '
      '"Chrome.Promo.Impression";',
    ]
    mock_file = self._create_mock_file(old_decls, new_decls)
    mock_input_api = MockInputApi()
    mock_input_api.files = [mock_file]
    results = PRESUBMIT.CheckNoNewSharedPreferencesCounters(
      mock_input_api, MockOutputApi()
    )
    self.assertEqual(1, len(results))
    self.assertEqual('warning', results[0].type)
    self.assertIn('NEW_TAB_PAGE_PROMO_SHOWN', results[0].message)
    self.assertIn('BOTTOM_SHEET_SEEN', results[0].message)
    self.assertIn('PROMO_IMPRESSION', results[0].message)

  def testNewKeyPrefixDeclarationWarns(self):
    old_decls = []
    new_decls = [
      '    public static final KeyPrefix TAB_GROUP_PROMO_SHOW_COUNT =',
      '            new KeyPrefix("Chrome.TabGroup.ShowCount.*");',
    ]
    mock_file = self._create_mock_file(old_decls, new_decls)
    mock_input_api = MockInputApi()
    mock_input_api.files = [mock_file]
    results = PRESUBMIT.CheckNoNewSharedPreferencesCounters(
      mock_input_api, MockOutputApi()
    )
    self.assertEqual(1, len(results))
    self.assertEqual('warning', results[0].type)
    self.assertIn('TAB_GROUP_PROMO_SHOW_COUNT', results[0].message)

  def testWindowsPathMatches(self):
    windows_path = (
      r'chrome\browser\preferences\android\java\src\org\chromium\chrome'
      r'\browser\preferences\ChromePreferenceKeys.java'
    )
    old_decls = []
    new_decls = [
      '    public static final String MY_FEATURE_VIEW_COUNT = '
      '"Chrome.MyFeature.ViewCount";',
    ]
    mock_file = self._create_mock_file(
      old_decls, new_decls, file_path=windows_path
    )
    mock_input_api = MockInputApi()
    mock_input_api.files = [mock_file]
    results = PRESUBMIT.CheckNoNewSharedPreferencesCounters(
      mock_input_api, MockOutputApi()
    )
    self.assertEqual(1, len(results))
    self.assertEqual('warning', results[0].type)
    self.assertIn('MY_FEATURE_VIEW_COUNT', results[0].message)


if __name__ == '__main__':
  unittest.main()
