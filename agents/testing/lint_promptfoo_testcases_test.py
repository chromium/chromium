#!/usr/bin/env vpython3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tests for lint_promptfoo_testcases."""

import os
import unittest
from unittest import mock

import lint_promptfoo_testcases


class LintPromptfooTestcasesTest(unittest.TestCase):

    def setUp(self):
        self.mock_file_path = 'agents/test.promptfoo.yaml'

    @mock.patch('lint_promptfoo_testcases._get_chromium_src_path',
                return_value='/mock/chromium/src')
    @mock.patch('os.path.exists', return_value=True)
    def test_valid_file_references(self, _mock_exists, _mock_get_src_path):
        """Tests that no errors are returned for a valid config."""
        data = {
            'providers': [{
                'config': {
                    'changes': [{
                        'apply': 'file://my_patch.diff'
                    }, {
                        'apply': 'file://my_patch_1.diff'
                    }],
                    'templates': ['file://my_template.txt'],
                    'extensions': ['my_extension']
                }
            }]
        }
        errors = lint_promptfoo_testcases.check_test_case(
            data, self.mock_file_path)
        self.assertEqual(len(errors), 0)

    @mock.patch('lint_promptfoo_testcases._get_chromium_src_path',
                return_value='/mock/chromium/src')
    @mock.patch('os.path.exists', return_value=False)
    def test_non_existent_file(self, mock_exists, _mock_get_src_path):
        """Tests that an error is returned for a non-existent file."""
        data = {
            'providers': [{
                'config': {
                    'changes': [{
                        'apply': 'file://agents/my_patch.diff'
                    }],
                }
            }]
        }
        errors = lint_promptfoo_testcases.check_test_case(
            data, self.mock_file_path)
        self.assertEqual(len(errors), 1)
        self.assertIn('refers to a non-existent file', errors[0])
        # Check that os.path.exists was called with the absolute path.
        expected_path = os.path.join('/mock/chromium/src', 'agents',
                                     'my_patch.diff')
        mock_exists.assert_called_with(expected_path)

    @mock.patch('lint_promptfoo_testcases._get_chromium_src_path',
                return_value='/mock/chromium/src')
    @mock.patch('os.path.exists', return_value=False)
    def test_non_existent_extension(self, mock_exists, _mock_get_src_path):
        """Tests that an error is returned for a non-existent extension."""
        data = {
            'providers': [{
                'config': {
                    'extensions': ['non_existent_extension']
                }
            }]
        }
        errors = lint_promptfoo_testcases.check_test_case(
            data, self.mock_file_path)
        self.assertEqual(len(errors), 1)
        self.assertIn('refers to a non-existent extension', errors[0])
        expected_path = os.path.join('/mock/chromium/src', 'agents',
                                     'extensions', 'non_existent_extension')
        mock_exists.assert_called_with(expected_path)

    def test_extension_with_file_path(self):
        """Tests that an error is returned for an extension with a file path."""
        data = {
            'providers': [{
                'config': {
                    'extensions': ['file://some/extension.py']
                }
            }]
        }
        errors = lint_promptfoo_testcases.check_test_case(
            data, self.mock_file_path)
        self.assertEqual(len(errors), 1)
        self.assertIn(
            'contains a file path for an extension. Please use '
            'the extension name instead', errors[0])

    def test_non_string_path(self):
        """Tests that a non-string path value returns an error."""
        data = {
            'providers': [{
                'config': {
                    'changes': [{
                        'apply': 12345
                    }],
                }
            }]
        }
        errors = lint_promptfoo_testcases.check_test_case(
            data, self.mock_file_path)
        self.assertEqual(len(errors), 1)
        self.assertIn('contains a non-string file reference', errors[0])

    def test_non_string_extension(self):
        """Tests that a non-string extension value returns an error."""
        data = {'providers': [{'config': {'extensions': [12345]}}]}
        errors = lint_promptfoo_testcases.check_test_case(
            data, self.mock_file_path)
        self.assertEqual(len(errors), 1)
        self.assertIn('contains a non-string extension reference', errors[0])

    def test_malformed_structure(self):
        """Tests that the linter doesn't crash on malformed data."""
        data = {
            'providers': [{
                'id': 'test',
                'config': 'not a dict'
            }],
        }
        # Should run without raising exceptions and return an error.
        errors = lint_promptfoo_testcases.check_test_case(
            data, self.mock_file_path)
        self.assertEqual(len(errors), 1)
        self.assertIn('"providers" field must have a dict "config" field',
                      errors[0])

    def test_provider_without_config(self):
        """Tests that a provider without a config field is valid."""
        data = {
            'providers': [{
                'id': 'test-provider-without-config',
            }]
        }
        errors = lint_promptfoo_testcases.check_test_case(
            data, self.mock_file_path)
        self.assertEqual(len(errors), 0)


if __name__ == '__main__':
    unittest.main()
