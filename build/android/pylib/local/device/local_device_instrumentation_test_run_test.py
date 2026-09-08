#!/usr/bin/env vpython3
# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for local_device_instrumentation_test_run."""

# pylint: disable=protected-access

import contextlib
import os
import random
import sys
import unittest
from unittest import mock

sys.path.append(
    os.path.abspath(os.path.join(os.path.dirname(__file__), '../../..'))
)

from pylib.base import base_test_result
from pylib.base import mock_environment
from pylib.base import mock_test_instance
from pylib.local.device import local_device_instrumentation_test_run


class LocalDeviceInstrumentationTestRunTest(unittest.TestCase):
    def setUp(self):
        super().setUp()
        self._env = mock_environment.MockEnvironment()
        self._env.trace_output = None
        self._ti = mock_test_instance.MockTestInstance()
        self._obj = local_device_instrumentation_test_run.LocalDeviceInstrumentationTestRun(
            self._env, self._ti
        )

    # TODO(crbug.com/41361955): Decide whether the _ShouldRetry hook is worth
    # retaining and remove these tests if not.

    def testShouldRetry_failure(self):
        test = {
            'annotations': {},
            'class': 'SadTest',
            'method': 'testFailure',
        }
        result = base_test_result.BaseTestResult(
            'SadTest.testFailure', base_test_result.ResultType.FAIL
        )
        self.assertTrue(self._obj._ShouldRetry(test, result))

    def testShouldRetry_retryOnFailure(self):
        test = {
            'annotations': {'RetryOnFailure': None},
            'class': 'SadTest',
            'method': 'testRetryOnFailure',
        }
        result = base_test_result.BaseTestResult(
            'SadTest.testRetryOnFailure', base_test_result.ResultType.FAIL
        )
        self.assertTrue(self._obj._ShouldRetry(test, result))

    def testShouldRetry_notRun(self):
        test = {
            'annotations': {},
            'class': 'SadTest',
            'method': 'testNotRun',
        }
        result = base_test_result.BaseTestResult(
            'SadTest.testNotRun', base_test_result.ResultType.NOTRUN
        )
        self.assertTrue(self._obj._ShouldRetry(test, result))

    def testIsRenderTest_matchedWithKey(self):
        test = {
            'annotations': {'Feature': {'value': ['RenderTest', 'dummy']}},
            'class': 'DummyTest',
            'method': 'testRun',
        }
        self.assertTrue(
            local_device_instrumentation_test_run._IsRenderTest(test)
        )

    def testIsRenderTest_noMatchedKey(self):
        test = {
            'annotations': {'Feature': {'value': ['abc', 'dummy']}},
            'class': 'DummyTest',
            'method': 'testRun',
        }
        self.assertFalse(
            local_device_instrumentation_test_run._IsRenderTest(test)
        )

    def testReplaceUncommonChars(self):
        original = 'abc#edf'
        self.assertEqual(
            local_device_instrumentation_test_run._ReplaceUncommonChars(
                original
            ),
            'abc__edf',
        )
        original = 'abc#edf#hhf'
        self.assertEqual(
            local_device_instrumentation_test_run._ReplaceUncommonChars(
                original
            ),
            'abc__edf__hhf',
        )
        original = 'abcedfhhf'
        self.assertEqual(
            local_device_instrumentation_test_run._ReplaceUncommonChars(
                original
            ),
            'abcedfhhf',
        )
        original = None
        with self.assertRaises(ValueError):
            local_device_instrumentation_test_run._ReplaceUncommonChars(
                original
            )
        original = ''
        with self.assertRaises(ValueError):
            local_device_instrumentation_test_run._ReplaceUncommonChars(
                original
            )

    def test_ApplyExternalSharding(self):
        test1_batch1 = create_test(
            {'Batch': {'value': 'batch1'}}, 'com.example.TestA', 'test1'
        )
        test2 = create_test(
            {'Features$EnableFeatures': {'value': 'defg'}},
            'com.example.TestB',
            'test2',
        )
        test3 = create_test({}, 'com.example.TestC', 'test3')
        test3_multiprocess = create_test(
            {}, 'com.example.TestC', 'test3__multiprocess_mode'
        )
        test4_batch1 = create_test(
            {'Batch': {'value': 'batch1'}}, 'com.example.TestD', 'test4'
        )
        test5_batch1 = create_test(
            {'Batch': {'value': 'batch1'}}, 'com.example.TestE', 'test5'
        )
        test6 = create_test({}, 'com.example.TestF', 'test6')
        test7 = create_test({}, 'com.example.TestG', 'test7')
        test8 = create_test({}, 'com.example.TestH', 'test8')

        tests = [
            test1_batch1,
            test2,
            test3,
            test3_multiprocess,
            test4_batch1,
            test5_batch1,
            test6,
            test7,
            test8,
        ]
        expected_shard0 = [test8]
        expected_shard1 = [
            [test1_batch1, test4_batch1, test5_batch1],
            test3_multiprocess,
            test7,
            test3,
            test6,
            test2,
        ]
        # Shuffle the tests two times to check if the output is deterministic.
        random.shuffle(tests)
        self.assertListEqual(
            self._obj._ApplyExternalSharding(tests, 0, 2), expected_shard0
        )
        self.assertListEqual(
            self._obj._ApplyExternalSharding(tests, 1, 2), expected_shard1
        )
        random.shuffle(tests)
        self.assertListEqual(
            self._obj._ApplyExternalSharding(tests, 0, 2), expected_shard0
        )
        self.assertListEqual(
            self._obj._ApplyExternalSharding(tests, 1, 2), expected_shard1
        )

    def test_GetTestsToRetry(self):
        test1_batch1 = create_test(
            {'Batch': {'value': 'batch1'}}, 'com.example.TestA', 'test1'
        )
        test2_batch1 = create_test(
            {'Batch': {'value': 'batch1'}}, 'com.example.TestB', 'test2'
        )
        test3 = create_test({}, 'com.example.TestC', 'test3')
        test4 = create_test({}, 'com.example.TestD', 'test4')
        test_data = [
            (test1_batch1, base_test_result.ResultType.PASS),
            (test2_batch1, base_test_result.ResultType.FAIL),
            (test3, base_test_result.ResultType.PASS),
            (test4, base_test_result.ResultType.FAIL),
        ]
        all_tests = [[test1_batch1, test2_batch1], test3, test4]
        try_results = base_test_result.TestRunResults()
        for test, test_result in test_data:
            try_results.AddResult(
                base_test_result.BaseTestResult(
                    self._obj._GetUniqueTestName(test), test_result
                )
            )
        actual_retry = self._obj._GetTestsToRetry(all_tests, try_results)
        expected_retry = [
            [test2_batch1],
            test4,
        ]
        self.assertListEqual(actual_retry, expected_retry)

    @mock.patch.object(
        local_device_instrumentation_test_run.LocalDeviceInstrumentationTestRun,
        '_ArchiveLogcat',
        return_value=contextlib.nullcontext(mock.MagicMock()),
    )
    @mock.patch('os.path.exists', return_value=True)
    @mock.patch(
        'pylib.utils.code_coverage_utils.PullAndMaybeMergeClangCoverageFiles'
    )
    def test_RunTest_coverage_force_main_user(
        self, mock_pull_clang, _mock_exists, _mock_logcat
    ):
        self._env.force_main_user = True
        self._ti.coverage_directory = '/host/coverage/dir'
        self._ti.apk_under_test = None
        self._ti.use_native_coverage_listener = False
        self._ti.enable_breakpad_dump = False
        self._ti.coverage_on_the_fly = False
        self._ti.GetRunDisabledFlag.return_value = False
        self._ti.is_unit_test = False
        self._ti.GetTimeout.return_value = 60
        self._ti.flags = []
        self._ti.package = 'com.example.test'
        self._ti.activity = 'TestActivity'
        self._ti.test_runner = 'TestRunner'

        device = mock.MagicMock()
        device.product_cpu_abi = 'x86_64'
        device.build_version_sdk = 34
        device.GetExternalStoragePath.return_value = '/sdcard'
        device.ResolveSpecialPath.side_effect = lambda p: (
            '/data/media/10' + p[len('/sdcard') :]
            if p.startswith('/sdcard')
            else p
        )
        device.PathExists.return_value = True
        device.StartInstrumentation.return_value = (0, ['OK'])

        test = {
            'class': 'com.example.MyTest',
            'method': 'testFoo',
            'annotations': {},
        }
        self._obj._RunTest(device, test)

        expected_jacoco_device_file = '/data/media/10/chrome/test/coverage/com.example.MyTest_testFoo.exec'
        device.PathExists.assert_any_call(
            expected_jacoco_device_file, as_root=True, retries=0
        )
        device.PullFile.assert_any_call(
            expected_jacoco_device_file, '/host/coverage/dir', as_root=True
        )
        device.RemovePath.assert_any_call(
            expected_jacoco_device_file, force=True, as_root=True
        )
        mock_pull_clang.assert_called_once_with(
            device,
            '/data/media/10/chrome/test/coverage/profraw',
            '/host/coverage/dir',
            'com.example.MyTest_testFoo',
            as_root=True,
        )

    @mock.patch.object(
        local_device_instrumentation_test_run.LocalDeviceInstrumentationTestRun,
        '_ArchiveLogcat',
        return_value=contextlib.nullcontext(mock.MagicMock()),
    )
    @mock.patch('os.path.exists', return_value=True)
    @mock.patch(
        'pylib.utils.code_coverage_utils.PullAndMaybeMergeClangCoverageFiles'
    )
    def test_RunTest_coverage_normal_user(
        self, mock_pull_clang, _mock_exists, _mock_logcat
    ):
        self._env.force_main_user = False
        self._ti.coverage_directory = '/host/coverage/dir'
        self._ti.apk_under_test = None
        self._ti.use_native_coverage_listener = False
        self._ti.enable_breakpad_dump = False
        self._ti.coverage_on_the_fly = False
        self._ti.GetRunDisabledFlag.return_value = False
        self._ti.is_unit_test = False
        self._ti.GetTimeout.return_value = 60
        self._ti.flags = []
        self._ti.package = 'com.example.test'
        self._ti.activity = 'TestActivity'
        self._ti.test_runner = 'TestRunner'

        device = mock.MagicMock()
        device.product_cpu_abi = 'x86_64'
        device.build_version_sdk = 34
        device.GetExternalStoragePath.return_value = '/sdcard'
        device.PathExists.return_value = True
        device.StartInstrumentation.return_value = (0, ['OK'])

        test = {
            'class': 'com.example.MyTest',
            'method': 'testFoo',
            'annotations': {},
        }
        self._obj._RunTest(device, test)

        expected_jacoco_device_file = (
            '/sdcard/chrome/test/coverage/com.example.MyTest_testFoo.exec'
        )
        device.PathExists.assert_any_call(
            expected_jacoco_device_file, as_root=False, retries=0
        )
        device.PullFile.assert_any_call(
            expected_jacoco_device_file, '/host/coverage/dir', as_root=False
        )
        device.RemovePath.assert_any_call(
            expected_jacoco_device_file, force=True, as_root=False
        )
        mock_pull_clang.assert_called_once_with(
            device,
            '/sdcard/chrome/test/coverage/profraw',
            '/host/coverage/dir',
            'com.example.MyTest_testFoo',
            as_root=False,
        )


def create_test(annotation_dict, class_name, method_name):
    # Helper function to generate test dict
    test = {
        'annotations': annotation_dict,
        'class': class_name,
        'method': method_name,
    }
    return test


if __name__ == '__main__':
    unittest.main(verbosity=2)
