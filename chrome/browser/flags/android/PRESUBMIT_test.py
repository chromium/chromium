#!/usr/bin/env python
# Copyright 2025 The Chromium Authors
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


class CheckCheckChromeFeatureListIsSorted(unittest.TestCase):
    """
    Tests the CheckChromeFeatureIsSorted PRESUBMIT check for export list.
    """
    FILE_PATH = 'chrome/browser/flags/android/chrome_feature_list.cc'
    NAME = 'kFeaturesExposedToJava'
    START_PATTERN = '// FEATURE_EXPORT_LIST_START'
    END_PATTERN = '// FEATURE_EXPORT_LIST_END'

    def _generate_file_content(self, features):
        """Helper to generate mock file content with a list of features."""
        content = [
            '// Some header content',
            '#include "some/header.h"',
            '',
            'namespace chrome {',
            'namespace android {',
            self.START_PATTERN,
        ]
        content.extend([f'    {feature}' for feature in features])
        content.extend([
            self.END_PATTERN,
            '}  // namespace android',
            '}  // namespace chrome',
        ])
        return content

    def testSortedListPasses(self):
        """Tests that a correctly sorted feature list passes the check."""
        features = [
            'autofill::features::kAutofillEnableSupportForHomeAndWork',
            'commerce::kCommerceMerchantViewer',
            'features::kAndroidBrowserControlsInViz',
            'kAdaptiveButtonInTopToolbarCustomizationV2',
        ]
        content = self._generate_file_content(features)

        mock_input_api = MockInputApi()
        mock_input_api.files = [MockAffectedFile(self.FILE_PATH, content)]
        results = PRESUBMIT.CheckChromeFeatureIsSorted(
            mock_input_api, MockOutputApi(), self.FILE_PATH, self.NAME,
            self.START_PATTERN, self.END_PATTERN)
        self.assertEqual([], results)

    def testUnsortedListFails(self):
        """
        Tests that an unsorted feature list fails the check with
        a specific error.
        """
        features = [
            'autofill::features::kAutofillEnableSupportForHomeAndWork',
            'features::kAndroidBrowser',  # Out of order
            'commerce::kCommerceMerchantViewer',
            'kAdaptiveButtonInTopToolbarCustomizationV2',
        ]
        content = self._generate_file_content(features)

        mock_input_api = MockInputApi()
        mock_input_api.files = [MockAffectedFile(self.FILE_PATH, content)]
        results = PRESUBMIT.CheckChromeFeatureIsSorted(
            mock_input_api, MockOutputApi(), self.FILE_PATH, self.NAME,
            self.START_PATTERN, self.END_PATTERN)

        self.assertEqual(1, len(results))
        self.assertEqual('error', results[0].type)
        self.assertIn("`kFeaturesExposedToJava`", results[0].message)
        self.assertIn("'features::kAndroidBrowser'",results[0].message)
        self.assertIn("'commerce::kCommerceMerchantViewer'", results[0].message)

    def testUnrelatedFileModifiedPasses(self):
        """Tests the check is skipped if the target file is not modified."""
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockAffectedFile('some/other/file.cc', ['// No changes here'])
        ]
        results = PRESUBMIT.CheckChromeFeatureIsSorted(
            mock_input_api, MockOutputApi(), self.FILE_PATH, self.NAME,
            self.START_PATTERN, self.END_PATTERN)
        self.assertEqual([], results)

    def testCaseSensitiveSortFails(self):
        """Tests sorting fails correctly for case-insensitive misordering."""
        # 'feature::A_feature' should come before 'feature::a_feature'.
        features = [
            'feature::a_feature',
            'feature::A_feature',
            'feature::c_feature',
        ]
        content = self._generate_file_content(features)
        mock_input_api = MockInputApi()
        mock_input_api.files = [MockAffectedFile(self.FILE_PATH, content)]
        results = PRESUBMIT.CheckChromeFeatureIsSorted(
            mock_input_api, MockOutputApi(), self.FILE_PATH, self.NAME,
            self.START_PATTERN, self.END_PATTERN)

        self.assertEqual(1, len(results))
        self.assertIn("'feature::A_feature'",results[0].message)
        self.assertIn("'feature::a_feature'",results[0].message)


class CheckChromeFeatureDefinitionsAreSortedTest(unittest.TestCase):
    """
    Tests the CheckChromeFeatureIsSorted PRESUBMIT check for definitions.
    """
    FILE_PATH = 'chrome/browser/flags/android/chrome_feature_list.cc'
    NAME = 'BASE_FEATURE'
    START_PATTERN = '// BASE_FEATURE_START'
    END_PATTERN = '// BASE_FEATURE_END'

    def _generate_file_content(self, feature_lines):
        """
        Helper to generate mock file content with a list of BASE_FEATURE
        definitions.
        """
        content = [
            '// Some header content',
            'namespace features {',
            self.START_PATTERN,
        ]
        content.extend(feature_lines)
        content.extend([
            self.END_PATTERN,
            '}  // namespace features',
        ])
        return content

    def testSortedListPasses(self):
        """Tests that a correctly sorted list of BASE_FEATUREs passes."""
        feature_lines = [
            'BASE_FEATURE(kAdaptiveFoo, base::FEATURE_ENABLED_BY_DEFAULT);',
            'BASE_FEATURE(kAndroidBar, base::FEATURE_ENABLED_BY_DEFAULT);',
            'BASE_FEATURE(kCommerce, base::FEATURE_ENABLED_BY_DEFAULT);',
            'BASE_FEATURE(kSomething, base::FEATURE_ENABLED_BY_DEFAULT);',
        ]
        content = self._generate_file_content(feature_lines)

        mock_input_api = MockInputApi()
        mock_input_api.files = [MockAffectedFile(self.FILE_PATH, content)]
        results = PRESUBMIT.CheckChromeFeatureIsSorted(
            mock_input_api, MockOutputApi(), self.FILE_PATH, self.NAME,
            self.START_PATTERN, self.END_PATTERN)
        self.assertEqual([], results)

    def testUnsortedListFails(self):
        """
        Tests that an unsorted list of BASE_FEATUREs fails the check with
        a specific error.
        """
        # kAdaptiveFoo comes alphabetically after kCommerce
        feature_lines = [
            # Out of order
            'BASE_FEATURE(kCommerce, base::FEATURE_ENABLED_BY_DEFAULT);',
            'BASE_FEATURE(kAdaptiveFoo, base::FEATURE_ENABLED_BY_DEFAULT);',
            'BASE_FEATURE(kAndroidBar, base::FEATURE_ENABLED_BY_DEFAULT);',
            'BASE_FEATURE(kSomething, base::FEATURE_ENABLED_BY_DEFAULT);',
        ]
        content = self._generate_file_content(feature_lines)

        mock_input_api = MockInputApi()
        mock_input_api.files = [MockAffectedFile(self.FILE_PATH, content)]
        results = PRESUBMIT.CheckChromeFeatureIsSorted(
            mock_input_api, MockOutputApi(), self.FILE_PATH, self.NAME,
            self.START_PATTERN, self.END_PATTERN)

        self.assertEqual(1, len(results))
        self.assertEqual('error', results[0].type)
        self.assertIn("`BASE_FEATURE` values", results[0].message)
        self.assertIn("'BASE_FEATURE(kCommerce", results[0].message)
        self.assertIn("'BASE_FEATURE(kAdaptiveFoo", results[0].message)

    def testUnrelatedFileModifiedPasses(self):
        """Tests the check is skipped if the target file is not modified."""
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockAffectedFile('some/other/file.cc', ['// No changes here'])
        ]
        results = PRESUBMIT.CheckChromeFeatureIsSorted(
            mock_input_api, MockOutputApi(), self.FILE_PATH, self.NAME,
            self.START_PATTERN, self.END_PATTERN)
        self.assertEqual([], results)

    def testCaseSensitiveSortFails(self):
        """Tests sorting fails correctly for case-insensitive misordering."""
        # 'A_feature' should come before 'a_feature' (case-insensitive)
        feature_lines = [
            # Out of order
            'BASE_FEATURE(a_feature, base::FEATURE_ENABLED_BY_DEFAULT);',
            'BASE_FEATURE(A_feature, base::FEATURE_ENABLED_BY_DEFAULT);',
            'BASE_FEATURE(B_feature, base::FEATURE_ENABLED_BY_DEFAULT);',
        ]
        content = self._generate_file_content(feature_lines)

        mock_input_api = MockInputApi()
        mock_input_api.files = [MockAffectedFile(self.FILE_PATH, content)]
        results = PRESUBMIT.CheckChromeFeatureIsSorted(
            mock_input_api, MockOutputApi(), self.FILE_PATH, self.NAME,
            self.START_PATTERN, self.END_PATTERN)

        self.assertEqual(1, len(results))
        self.assertIn("'BASE_FEATURE(A_feature", results[0].message)
        self.assertIn("'BASE_FEATURE(a_feature", results[0].message)


class CheckChromeFeatureDeclarationsHeaderIsSortedTest(unittest.TestCase):
    """
    Tests the CheckChromeFeatureIsSorted PRESUBMIT check for declarations.
    """
    FILE_PATH = 'chrome/browser/flags/android/chrome_feature_list.h'
    NAME = 'BASE_DECLARE_FEATURE'
    START_PATTERN = '// BASE_DECLARE_FEATURE_START'
    END_PATTERN = '// BASE_DECLARE_FEATURE_END'

    def _generate_file_content(self, declare_names):
        """
        Helper to generate mock file content with a list of
        BASE_DECLARE_FEATURE declarations.
        """
        content = [
            '// Some header content',
            '#define CHROME_BROWSER_FLAGS_ANDROID_CHROME_FEATURE_LIST_H_',
            'namespace features {',
            self.START_PATTERN,
        ]
        content.extend([f'BASE_DECLARE_FEATURE({d});' for d in declare_names])
        content.extend([
            self.END_PATTERN,
            '}  // namespace features',
        ])
        return content

    def testSortedListPasses(self):
        """
        Tests that a correctly sorted list of BASE_DECLARE_FEATUREs passes.
        """
        declare_names = [
            'kAdaptiveButtonInTopToolbarCustomizationV2',
            'kAndroidBrowserControlsInViz',
            'kCommerceMerchantViewer',
            'kSomeOtherFeature',
        ]
        content = self._generate_file_content(declare_names)

        mock_input_api = MockInputApi()
        mock_input_api.files = [MockAffectedFile(self.FILE_PATH, content)]
        results = PRESUBMIT.CheckChromeFeatureIsSorted(
            mock_input_api, MockOutputApi(), self.FILE_PATH, self.NAME,
            self.START_PATTERN, self.END_PATTERN)
        self.assertEqual([], results)

    def testUnsortedListFails(self):
        """
        Tests that an unsorted list of BASE_DECLARE_FEATUREs fails the check with
        a specific error.
        """
        # kCommerce comes alphabetically after kAndroid
        declare_names = [
            'kAdaptiveButtonInTopToolbarCustomizationV2',
            'kCommerce',  # Out of order
            'kAndroid',
            'kSomeOtherFeature',
        ]
        content = self._generate_file_content(declare_names)

        mock_input_api = MockInputApi()
        mock_input_api.files = [MockAffectedFile(self.FILE_PATH, content)]
        results = PRESUBMIT.CheckChromeFeatureIsSorted(
            mock_input_api, MockOutputApi(), self.FILE_PATH, self.NAME,
            self.START_PATTERN, self.END_PATTERN)

        self.assertEqual(1, len(results))
        self.assertEqual('error', results[0].type)
        self.assertIn("`BASE_DECLARE_FEATURE` values", results[0].message)
        self.assertIn("'BASE_DECLARE_FEATURE(kCommerce);'", results[0].message)
        self.assertIn("'BASE_DECLARE_FEATURE(kAndroid);'", results[0].message)


    def testUnrelatedFileModifiedPasses(self):
        """Tests the check is skipped if the target file is not modified."""
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockAffectedFile('some/other/file.cc', ['// No changes here'])
        ]
        results = PRESUBMIT.CheckChromeFeatureIsSorted(
            mock_input_api, MockOutputApi(), self.FILE_PATH, self.NAME,
            self.START_PATTERN, self.END_PATTERN)
        self.assertEqual([], results)

    def testCaseSensitiveSortFails(self):
        """Tests sorting fails correctly for case-insensitive misordering."""
        # 'A_flag' should come before 'a_flag' (case-insensitive)
        declare_names = [
            'a_flag',
            'A_flag', # Out of order
            'c_flag',
        ]
        content = self._generate_file_content(declare_names)
        mock_input_api = MockInputApi()
        mock_input_api.files = [MockAffectedFile(self.FILE_PATH, content)]
        results = PRESUBMIT.CheckChromeFeatureIsSorted(
            mock_input_api, MockOutputApi(), self.FILE_PATH, self.NAME,
            self.START_PATTERN, self.END_PATTERN)

        self.assertEqual(1, len(results))
        self.assertIn("'BASE_DECLARE_FEATURE(A_flag);'", results[0].message)
        self.assertIn("'BASE_DECLARE_FEATURE(a_flag);'", results[0].message)


if __name__ == '__main__':
    unittest.main()

