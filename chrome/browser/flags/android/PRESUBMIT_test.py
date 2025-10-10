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
    Tests the CheckChromeFeatureListIsSorted PRESUBMIT check.
    """
    FILE_PATH = 'chrome/browser/flags/android/chrome_feature_list.cc'
    ARRAY_START = 'const base::Feature* const kFeaturesExposedToJava[] = {'
    ARRAY_END = '};'

    def _generate_file_content(self, features):
        """Helper to generate mock file content with a list of features."""
        content = [
            '// Some header content',
            '#include "some/header.h"',
            '',
            'namespace chrome {',
            'namespace android {',
            '',
            self.ARRAY_START,
        ]
        content.extend([f'    &{feature},' for feature in features])
        content.append(self.ARRAY_END)
        content.extend([
            '',
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
        results = PRESUBMIT.CheckChromeFeatureListIsSorted(
            mock_input_api, MockOutputApi())
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
        results = PRESUBMIT.CheckChromeFeatureListIsSorted(
            mock_input_api, MockOutputApi())

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
        results = PRESUBMIT.CheckChromeFeatureListIsSorted(
            mock_input_api, MockOutputApi())
        self.assertEqual([], results)

    def testCaseInsensitiveSortPasses(self):
        """Tests that sorting is case-insensitive as intended."""
        # This list is sorted correctly if you ignore case.
        features = [
            'feature::a_feature',
            'feature::B_feature',
            'feature::c_feature',
        ]
        content = self._generate_file_content(features)
        mock_input_api = MockInputApi()
        mock_input_api.files = [MockAffectedFile(self.FILE_PATH, content)]
        results = PRESUBMIT.CheckChromeFeatureListIsSorted(
            mock_input_api, MockOutputApi())
        self.assertEqual([], results)

    def testCaseInsensitiveSortFails(self):
        """Tests sorting fails correctly for case-insensitive misordering."""
        # 'a_feature' should come before 'B_feature'.
        features = [
            'feature::B_feature',
            'feature::a_feature',
            'feature::c_feature',
        ]
        content = self._generate_file_content(features)
        mock_input_api = MockInputApi()
        mock_input_api.files = [MockAffectedFile(self.FILE_PATH, content)]
        results = PRESUBMIT.CheckChromeFeatureListIsSorted(
            mock_input_api, MockOutputApi())

        self.assertEqual(1, len(results))
        self.assertIn("'feature::B_feature'",results[0].message)
        self.assertIn("'feature::a_feature'",results[0].message)


if __name__ == '__main__':
    unittest.main()

