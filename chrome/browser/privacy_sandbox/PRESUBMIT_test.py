# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import unittest

import PRESUBMIT

# Append chrome source root to import `PRESUBMIT_test_mocks.py`.
sys.path.append(
    os.path.dirname(
        os.path.dirname(
            os.path.dirname(os.path.dirname(os.path.abspath(__file__))))))
import PRESUBMIT_test_mocks

SAMPLE_FILE_CONTENT = (
                    f'<?xml version="1.0" encoding="utf-8"?>'
                    '<LinearLayout'
                    '   xmlns:android="http://schemas.android.com/apk/res/android"'
                    '   xmlns:app="http://schemas.android.com/apk/res-auto"'
                    '   android:id="@+id/privacy_sandbox_dialog">'
                    '   {elements}'
                    '</LinearLayout>')
PS_NOTICE_EEA_FILE = (
    'chrome/browser/privacy_sandbox/android/java/res/layout/'
    'privacy_sandbox_notice_eea.xml')
PS_NOTICE_ROW_FILE = (
    'chrome/browser/privacy_sandbox/android/java/res/layout/'
    'privacy_sandbox_notice_row.xml')
PS_NOTICE_EEA_TEXT = 'android:text="@string/privacy_sandbox_notice_eea"'
PS_NOTICE_ROW_TEXT = 'android:text="@string/privacy_sandbox_notice_row"'
PS_NOTICE_LOGO_ATTRIBUTE = (
  'android:layout_gravity="center"\n\t'
  'app:srcCompat="@drawable/chrome_sync_logo"')


class CheckPrivacySandboxXmlElementsHaveResourceIdsTest(unittest.TestCase):
    """Test the _CheckPrivacySandboxXmlElementsHaveResourceIds presubmit."""

    def testNoAffectedFiles(self):
        mock_input_api = PRESUBMIT_test_mocks.MockInputApi()
        mock_output_api = PRESUBMIT_test_mocks.MockOutputApi()
        result = PRESUBMIT.CheckPrivacySandboxXmlElementsHaveResourceIds(
            mock_input_api, mock_output_api)
        self.assertEqual(result, [])

    def testAffectedFileHasOneElementWithMissingId(self):
        mock_input_api = PRESUBMIT_test_mocks.MockInputApi()
        mock_input_api.files = [
            PRESUBMIT_test_mocks.MockFile(
                PS_NOTICE_EEA_FILE,
                [SAMPLE_FILE_CONTENT.format(
                elements =
                f'<TextView'
                f'    {PS_NOTICE_EEA_TEXT} />')
                ],
            ),
        ]
        mock_output_api = PRESUBMIT_test_mocks.MockOutputApi()
        result = PRESUBMIT.CheckPrivacySandboxXmlElementsHaveResourceIds(
            mock_input_api, mock_output_api)
        self.assertEqual(len(result), 1)
        self.assertEqual(result[0].type, 'warning')

        affected_file = result[0].items[1]
        bad_element = result[0].items[2]
        self.assertEqual(PS_NOTICE_EEA_FILE, affected_file)
        self.assertEqual(
            f'<TextView\n\t{PS_NOTICE_EEA_TEXT}/>', bad_element)

    def testAffectedFileHasMultipleElementsWithMissingId(self):
        mock_input_api = PRESUBMIT_test_mocks.MockInputApi()
        mock_input_api.files = [
            PRESUBMIT_test_mocks.MockFile(
                PS_NOTICE_EEA_FILE,
                [SAMPLE_FILE_CONTENT.format(
                    elements =
                    f'<TextView'
                    f'    {PS_NOTICE_EEA_TEXT} />'
                    f'<ImageView'
                    f'    {PS_NOTICE_LOGO_ATTRIBUTE} />')
                ],
            ),
        ]
        mock_output_api = PRESUBMIT_test_mocks.MockOutputApi()
        result = PRESUBMIT.CheckPrivacySandboxXmlElementsHaveResourceIds(
            mock_input_api, mock_output_api)
        self.assertEqual(len(result), 1)
        self.assertEqual(result[0].type, 'warning')

        affected_file, bad_element_1, bad_element_2 = result[0].items[1:4]
        self.assertEqual(PS_NOTICE_EEA_FILE, affected_file)
        self.assertEqual(
            f'<TextView\n\t{PS_NOTICE_EEA_TEXT}/>', bad_element_1)
        self.assertEqual(
            f'<ImageView\n\t{PS_NOTICE_LOGO_ATTRIBUTE}/>', bad_element_2)

    def testAffectedFileHasOneElementWithNoAttributes(self):
        mock_input_api = PRESUBMIT_test_mocks.MockInputApi()
        mock_input_api.files = [
            PRESUBMIT_test_mocks.MockFile(
                PS_NOTICE_EEA_FILE,
                [SAMPLE_FILE_CONTENT.format(
                    elements =
                    '<TextView/>')
                ],
            ),
        ]
        mock_output_api = PRESUBMIT_test_mocks.MockOutputApi()
        result = PRESUBMIT.CheckPrivacySandboxXmlElementsHaveResourceIds(
            mock_input_api, mock_output_api)
        self.assertEqual(len(result), 1)
        self.assertEqual(result[0].type, 'warning')

        affected_file = result[0].items[1]
        bad_element_1 = result[0].items[2]
        self.assertEqual(PS_NOTICE_EEA_FILE, affected_file)
        self.assertEqual('<TextView/>', bad_element_1)

    def testAffectedFileHasNoMissingIds(self):
        mock_input_api = PRESUBMIT_test_mocks.MockInputApi()
        mock_input_api.files = [
            PRESUBMIT_test_mocks.MockFile(
                PS_NOTICE_EEA_FILE,
                [SAMPLE_FILE_CONTENT.format(
                    elements =
                    f'<TextView'
                    f'    android:id="@+id/eea_header"'
                    f'    {PS_NOTICE_EEA_TEXT} />'
                    f'<ImageView'
                    f'    android:id="@+id/chrome_eea_logo"'
                    f'    {PS_NOTICE_LOGO_ATTRIBUTE} />')
                ],
            ),
        ]
        mock_output_api = PRESUBMIT_test_mocks.MockOutputApi()
        result = PRESUBMIT.CheckPrivacySandboxXmlElementsHaveResourceIds(
            mock_input_api, mock_output_api)
        self.assertEqual(result, [])

    def testAffectedFileHasNestedElementWithMissingId(self):
        mock_input_api = PRESUBMIT_test_mocks.MockInputApi()
        mock_input_api.files = [
            PRESUBMIT_test_mocks.MockFile(
                PS_NOTICE_EEA_FILE,
                [SAMPLE_FILE_CONTENT.format(
                    elements =
                    f'<TextView'
                    f'    android:id="@+id/eea_header"'
                    f'    {PS_NOTICE_EEA_TEXT} />'
                    f'<LinearLayout'
                    f'    android:id="@+id/privacy_sandbox_notice_eea_content">'
                    f'    <TextView'
                    f'        android:text="@string/nested_element_text" />'
                    f'</LinearLayout>'
                    f'<ImageView'
                    f'    android:id="@+id/chrome_eea_logo"'
                    f'    {PS_NOTICE_LOGO_ATTRIBUTE} />')
                ],
            ),
        ]
        mock_output_api = PRESUBMIT_test_mocks.MockOutputApi()
        result = PRESUBMIT.CheckPrivacySandboxXmlElementsHaveResourceIds(
            mock_input_api, mock_output_api)
        self.assertEqual(len(result), 1)
        self.assertEqual(result[0].type, 'warning')

        affected_file = result[0].items[1]
        bad_element_1 = result[0].items[2]
        self.assertEqual(PS_NOTICE_EEA_FILE, affected_file)
        self.assertEqual(
            f'<TextView\n\tandroid:text="@string/nested_element_text"/>',
            bad_element_1)

    def testAffectedFileHasMultipleElementsWithMissingIdInDifferentFiles(self):
        mock_input_api = PRESUBMIT_test_mocks.MockInputApi()
        mock_input_api.files = [
            PRESUBMIT_test_mocks.MockFile(
                PS_NOTICE_EEA_FILE,
                [SAMPLE_FILE_CONTENT.format(
                    elements =
                    f'<TextView'
                    f'    {PS_NOTICE_EEA_TEXT} />'
                    f'<ImageView'
                    f'    {PS_NOTICE_LOGO_ATTRIBUTE} />')
                ],
            ),
            PRESUBMIT_test_mocks.MockFile(
                PS_NOTICE_ROW_FILE,
                [SAMPLE_FILE_CONTENT.format(
                    elements =
                    f'<TextView'
                    f'    {PS_NOTICE_ROW_TEXT} />'
                    f'<ImageView'
                    f'    {PS_NOTICE_LOGO_ATTRIBUTE} />')
                ],
            ),
        ]
        mock_output_api = PRESUBMIT_test_mocks.MockOutputApi()
        result = PRESUBMIT.CheckPrivacySandboxXmlElementsHaveResourceIds(
            mock_input_api, mock_output_api)
        self.assertEqual(len(result), 1)
        self.assertEqual(result[0].type, 'warning')

        affected_file, bad_element_1, bad_element_2 = result[0].items[1:4]
        self.assertEqual(PS_NOTICE_EEA_FILE, affected_file)
        self.assertEqual(
            f'<TextView\n\t{PS_NOTICE_EEA_TEXT}/>', bad_element_1)
        self.assertEqual(
            f'<ImageView\n\t{PS_NOTICE_LOGO_ATTRIBUTE}/>', bad_element_2)

        affected_file, bad_element_1, bad_element_2 = result[0].items[5:8]
        self.assertEqual(PS_NOTICE_ROW_FILE, affected_file)
        self.assertEqual(
            f'<TextView\n\t{PS_NOTICE_ROW_TEXT}/>', bad_element_1)
        self.assertEqual(
            f'<ImageView\n\t{PS_NOTICE_LOGO_ATTRIBUTE}/>', bad_element_2)