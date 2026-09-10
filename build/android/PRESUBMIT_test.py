#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import unittest
from unittest import mock

_HERE = os.path.dirname(os.path.abspath(__file__))
_SRC_ROOT = os.path.normpath(os.path.join(_HERE, '..', '..'))

sys.path.insert(0, _HERE)
sys.path.append(_SRC_ROOT)

import PRESUBMIT
from PRESUBMIT_test_mocks import MockFile, MockInputApi, MockOutputApi


class AndroidPresubmitTest(unittest.TestCase):
    def setUp(self):
        self.mock_input_api = MockInputApi()
        self.mock_input_api.environ = {}
        self.mock_input_api.PresubmitLocalPath = lambda: _HERE
        self.mock_output_api = MockOutputApi()

    def _getAllTests(self):
        self.mock_input_api.no_diffs = True
        all_tests = PRESUBMIT.GetScopedUnitTests(
            self.mock_input_api, _HERE, is_upload=True
        )
        self.mock_input_api.no_diffs = False
        return all_tests

    def testNoDiffsRunsAllTests(self):
        self.mock_input_api.no_diffs = True
        tests = PRESUBMIT.GetScopedUnitTests(
            self.mock_input_api, _HERE, is_upload=True
        )
        self.assertTrue(tests)
        self.assertTrue(any('gyp' in t for t in tests))
        self.assertTrue(any('pylib' in t for t in tests))

    def testPresubmitSelfModifiedRunsAllTests(self):
        self.mock_input_api.files = [
            MockFile(os.path.join(_HERE, 'PRESUBMIT.py'), ['# edit'])
        ]
        tests = PRESUBMIT.GetScopedUnitTests(
            self.mock_input_api, _HERE, is_upload=True
        )
        self.assertEqual(self._getAllTests(), tests)

    def testPresubmitTestModifiedRunsAllTests(self):
        self.mock_input_api.files = [
            MockFile(os.path.join(_HERE, 'PRESUBMIT_test.py'), ['# edit'])
        ]
        tests = PRESUBMIT.GetScopedUnitTests(
            self.mock_input_api, _HERE, is_upload=True
        )
        self.assertEqual(self._getAllTests(), tests)

    def testUnrelatedFileSkipsAllTests(self):
        self.mock_input_api.files = [
            MockFile(os.path.join(_HERE, 'DIR_METADATA'), ['# meta']),
            MockFile(os.path.join(_HERE, 'resource_sizes.gni'), ['# edit']),
        ]
        tests = PRESUBMIT.GetScopedUnitTests(
            self.mock_input_api, _HERE, is_upload=True
        )
        self.assertEqual([], tests)

    def testPrefixCollisionDoesNotMatch(self):
        self.mock_input_api.files = [
            MockFile(os.path.join(_HERE, 'gyp_foo.py.bak'), ['# edit']),
            MockFile(os.path.join(_HERE, 'test_runner.txt'), ['# edit']),
        ]
        tests = PRESUBMIT.GetScopedUnitTests(
            self.mock_input_api, _HERE, is_upload=True
        )
        self.assertEqual([], tests)

    def testGypFileRunsGypTestsOnly(self):
        self.mock_input_api.files = [
            MockFile(os.path.join(_HERE, 'gyp', 'compile_java.py'), ['# edit'])
        ]
        tests = PRESUBMIT.GetScopedUnitTests(
            self.mock_input_api, _HERE, is_upload=True
        )
        self.assertTrue(tests)
        self.assertTrue(all('gyp' in t for t in tests))
        self.assertFalse(any('pylib' in t for t in tests))

    def testGypSubdirFileRunsGypTests(self):
        self.mock_input_api.files = [
            MockFile(
                os.path.join(_HERE, 'gyp', 'util', 'md5_check.py'), ['# edit']
            )
        ]
        tests = PRESUBMIT.GetScopedUnitTests(
            self.mock_input_api, _HERE, is_upload=True
        )
        self.assertTrue(tests)
        self.assertTrue(all('gyp' in t for t in tests))

    def testPylibFileRunsPylibTestsOnly(self):
        self.mock_input_api.files = [
            MockFile(
                os.path.join(_HERE, 'pylib', 'base', 'output_manager.py'),
                ['# edit'],
            )
        ]
        tests = PRESUBMIT.GetScopedUnitTests(
            self.mock_input_api, _HERE, is_upload=True
        )
        self.assertTrue(tests)
        self.assertTrue(
            all('pylib' in t or 'test_runner_test.py' in t for t in tests)
        )
        self.assertFalse(any('gyp' in t for t in tests))

    def testRootDirectoryFileRunsRootTestsOnly(self):
        self.mock_input_api.files = [
            MockFile(os.path.join(_HERE, 'convert_dex_profile.py'), ['# edit'])
        ]
        tests = PRESUBMIT.GetScopedUnitTests(
            self.mock_input_api, _HERE, is_upload=True
        )
        self.assertTrue(tests)
        self.assertTrue(
            any(t.endswith('convert_dex_profile_tests.py') for t in tests)
        )
        self.assertTrue(
            any(
                t.endswith('list_class_verification_failures_test.py')
                for t in tests
            )
        )
        self.assertTrue(
            any(t.endswith('fast_local_dev_server_test.py') for t in tests)
        )
        self.assertFalse(any('gyp' in t for t in tests))
        self.assertFalse(any('pylib' in t for t in tests))

    def testRootTestsExcludeUploadOnlyOnCommit(self):
        self.mock_input_api.files = [
            MockFile(os.path.join(_HERE, 'resource_sizes.py'), ['# edit'])
        ]
        tests = PRESUBMIT.GetScopedUnitTests(
            self.mock_input_api, _HERE, is_upload=False
        )
        self.assertTrue(tests)
        self.assertFalse(
            any(t.endswith('fast_local_dev_server_test.py') for t in tests)
        )

    def testMultipleDirectoriesModified(self):
        self.mock_input_api.files = [
            MockFile(os.path.join(_HERE, 'gyp', 'compile_java.py'), ['# edit']),
            MockFile(
                os.path.join(_HERE, 'pylib', 'base', 'output_manager.py'),
                ['# edit'],
            ),
        ]
        tests = PRESUBMIT.GetScopedUnitTests(
            self.mock_input_api, _HERE, is_upload=True
        )
        self.assertTrue(any('gyp' in t for t in tests))
        self.assertTrue(any('pylib' in t for t in tests))
        self.assertFalse(
            any(t.endswith('convert_dex_profile_tests.py') for t in tests)
        )

    def testFileDeletionRunsTests(self):
        self.mock_input_api.files = [
            MockFile(
                os.path.join(_HERE, 'gyp', 'compile_java.py'),
                [],
                action='D',
            )
        ]
        tests = PRESUBMIT.GetScopedUnitTests(
            self.mock_input_api, _HERE, is_upload=True
        )
        self.assertTrue(tests)
        self.assertTrue(all('gyp' in t for t in tests))
        self.assertFalse(any('pylib' in t for t in tests))

    def testDirectlyModifiedTestFileIncluded(self):
        self.mock_input_api.files = [
            MockFile(
                os.path.join(_HERE, 'gyp', 'dex_test.py'),
                ['# edit'],
            )
        ]
        tests = PRESUBMIT.GetScopedUnitTests(
            self.mock_input_api, _HERE, is_upload=True
        )
        self.assertIn(os.path.join(_HERE, 'gyp', 'dex_test.py'), tests)

    def testCommonChecksSkipsOnWindows(self):
        self.mock_input_api.sys = mock.MagicMock()
        self.mock_input_api.sys.platform = 'win32'
        results = PRESUBMIT.CommonChecks(
            self.mock_input_api, self.mock_output_api, is_upload=True
        )
        self.assertEqual([], results)

    def testCommonChecksInvokesPylintAndScopedUnitTests(self):
        self.mock_input_api.sys = mock.MagicMock()
        self.mock_input_api.sys.platform = 'linux'
        self.mock_input_api.canned_checks.GetPylint = mock.MagicMock(
            return_value=['pylint_check']
        )
        self.mock_input_api.canned_checks.GetUnitTests = mock.MagicMock(
            return_value=['unit_test_check']
        )
        self.mock_input_api.RunTests = mock.MagicMock(return_value=['passed'])
        self.mock_input_api.files = [
            MockFile(os.path.join(_HERE, 'PRESUBMIT.py'), ['# edit'])
        ]
        results = PRESUBMIT.CommonChecks(
            self.mock_input_api, self.mock_output_api, is_upload=True
        )
        self.assertEqual(
            2, self.mock_input_api.canned_checks.GetPylint.call_count
        )
        self.assertEqual(
            1, self.mock_input_api.canned_checks.GetUnitTests.call_count
        )
        self.mock_input_api.RunTests.assert_called_once_with(
            ['pylint_check', 'pylint_check', 'unit_test_check']
        )
        self.assertEqual(['passed'], results)


if __name__ == '__main__':
    unittest.main()
