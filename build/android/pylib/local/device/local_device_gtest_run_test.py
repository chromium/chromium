#!/usr/bin/env vpython3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tests for local_device_gtest_test_run."""

# pylint: disable=protected-access

import os
import random
import sys
import unittest
from unittest import mock

sys.path.append(
    os.path.abspath(os.path.join(os.path.dirname(__file__), '../../..'))
)

from pylib.base import base_test_result
from pylib.gtest import gtest_test_instance
from pylib.local.device import local_device_environment
from pylib.local.device import local_device_gtest_run


def isSliceInList(s, l):
    lenOfSlice = len(s)
    return any(
        s == l[i : lenOfSlice + i] for i in range(len(l) - lenOfSlice + 1)
    )


class LocalDeviceGtestRunTest(unittest.TestCase):
    def setUp(self):
        self._obj = local_device_gtest_run.LocalDeviceGtestRun(
            mock.MagicMock(
                spec=local_device_environment.LocalDeviceEnvironment
            ),
            mock.MagicMock(spec=gtest_test_instance.GtestTestInstance),
        )

    def testExtractTestsFromFilter(self):
        # Checks splitting by colons.
        self.assertEqual(
            set(
                [
                    'm4e3',
                    'p51',
                    'b17',
                ]
            ),
            set(
                local_device_gtest_run._ExtractTestsFromFilters(
                    ['b17:m4e3:p51']
                )
            ),
        )
        # Checks the '-' sign.
        self.assertIsNone(
            local_device_gtest_run._ExtractTestsFromFilters(['-mk2'])
        )
        # Checks the more than one asterick.
        self.assertIsNone(
            local_device_gtest_run._ExtractTestsFromFilters(['.mk2*:.M67*'])
        )
        # Checks just an asterick without a period
        self.assertIsNone(
            local_device_gtest_run._ExtractTestsFromFilters(['M67*'])
        )
        # Checks an asterick at the end with a period.
        self.assertEqual(
            ['.M67*'],
            local_device_gtest_run._ExtractTestsFromFilters(['.M67*']),
        )
        # Checks multiple filters intersect
        self.assertEqual(
            ['m4e3'],
            local_device_gtest_run._ExtractTestsFromFilters(
                ['b17:m4e3:p51', 'b17:m4e3', 'm4e3:p51']
            ),
        )

    def testGetLLVMProfilePath(self):
        path = local_device_gtest_run._GetLLVMProfilePath(
            'test_dir', 'sr71', '5'
        )
        self.assertEqual(path, os.path.join('test_dir', 'sr71_5_%2m%c.profraw'))

    def testGroupPreTests(self):
        pre_test_group1 = [
            'TestSuite1.PRE_PRE_TestName',
            'TestSuite1.PRE_TestName',
            'TestSuite1.TestName',
        ]
        # pre test group with "abc_" added to pre_test_group1
        pre_test_group2 = [
            'TestSuite1.PRE_PRE_abc_TestName',
            'TestSuite1.PRE_abc_TestName',
            'TestSuite1.abc_TestName',
        ]
        # pre test group with disabled prefixes in test name
        pre_test_group3 = [
            'TestSuite2.DISABLED_PRE_PRE_TestName',
            'TestSuite2.DISABLED_PRE_TestName',
            'TestSuite2.DISABLED_TestName',
        ]
        # pre test group with disabled prefixes in test suite
        pre_test_group4 = [
            'DISABLED_TestSuite3.PRE_PRE_TestName',
            'DISABLED_TestSuite3.PRE_TestName',
            'DISABLED_TestSuite3.TestName',
        ]

        tests = (
            [
                'TestSuite1.NormalTestName1',
                'TestSuite1.NormalTestName2',
            ]
            + pre_test_group1
            + pre_test_group2
            + pre_test_group3
            + pre_test_group4
        )
        # shuffle the tests to test if each pre test group is still in order.
        random.shuffle(tests)
        pre_tests, _ = local_device_gtest_run._GroupPreTests(tests)
        self.assertIn(pre_test_group1, pre_tests)
        self.assertIn(pre_test_group2, pre_tests)
        self.assertIn(pre_test_group3, pre_tests)
        self.assertIn(pre_test_group4, pre_tests)

    def test_IsPreTestGroup(self):
        self.assertTrue(
            local_device_gtest_run._IsPreTestGroup(
                [
                    'TestSuite1.TestName1',
                    'TestSuite1.PRE_TestName1',
                    'TestSuite1.PRE_PRE_TestName1',
                ]
            )
        )
        self.assertFalse(
            local_device_gtest_run._IsPreTestGroup(
                [
                    'TestSuite1.TestName2',
                ]
            )
        )
        self.assertFalse(
            local_device_gtest_run._IsPreTestGroup(
                [
                    'TestSuite1.TestName1',
                    'TestSuite1.PRE_TestName1',
                    'TestSuite1.TestName2',
                ]
            )
        )

    def test_ApplyExternalSharding(self):
        tests = [
            'TestSuite1.TestName1',
            'TestSuite1.TestName2',
            'TestSuite1.PRE_TestName3',
            'TestSuite1.TestName3',
            'TestSuite2.PRE_TestName1',
            'TestSuite2.TestName1',
            'TestSuite2.TestName2',
        ]
        expected_shard0 = [
            'TestSuite1.TestName2',
            'TestSuite1.TestName1',
        ]
        expected_shard1 = [
            ['TestSuite1.PRE_TestName3', 'TestSuite1.TestName3'],
            ['TestSuite2.PRE_TestName1', 'TestSuite2.TestName1'],
            'TestSuite2.TestName2',
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

    def test_CreateShardsForDevices(self):
        self._obj._env.devices = [1]
        self._obj._test_instance.test_launcher_batch_limit = 2
        self._obj._crashes = set(
            ['TestSuite1.CrashedTest1', 'TestSuite1.CrashedTest2']
        )
        tests = [
            ['TestSuite1.PRE_TestName1', 'TestSuite1.TestName1'],
            'TestSuite1.TestName2',
            'TestSuite1.TestName3',
            'TestSuite1.CrashedTest1',
            'TestSuite2.TestName1',
            ['TestSuite1.PRE_CrashedTest2', 'TestSuite1.CrashedTest2'],
            'TestSuite2.TestName2',
            'TestSuite2.TestName3',
        ]
        expected_shards = [
            ['TestSuite1.PRE_TestName1', 'TestSuite1.TestName1'],
            ['TestSuite1.TestName2', 'TestSuite1.TestName3'],
            ['TestSuite1.CrashedTest1'],
            ['TestSuite1.PRE_CrashedTest2', 'TestSuite1.CrashedTest2'],
            ['TestSuite2.TestName1', 'TestSuite2.TestName2'],
            ['TestSuite2.TestName3'],
        ]
        actual_shards = self._obj._CreateShardsForDevices(tests)
        self.assertListEqual(actual_shards, expected_shards)

    def test_GetTestsToRetry(self):
        test_data = [
            ('TestSuite1.TestName1', base_test_result.ResultType.PASS),
            ('TestSuite1.TestName2', base_test_result.ResultType.FAIL),
            ('TestSuite1.PRE_TestName3', base_test_result.ResultType.PASS),
            ('TestSuite1.TestName3', base_test_result.ResultType.FAIL),
        ]
        all_tests = [
            'TestSuite1.TestName1',
            'TestSuite1.TestName2',
            ['TestSuite1.PRE_TestName3', 'TestSuite1.TestName3'],
        ]
        try_results = base_test_result.TestRunResults()
        for test, test_result in test_data:
            try_results.AddResult(
                base_test_result.BaseTestResult(
                    self._obj._GetUniqueTestName(test), test_result
                )
            )
        actual_retry = self._obj._GetTestsToRetry(all_tests, try_results)
        expected_retry = [
            'TestSuite1.TestName2',
            ['TestSuite1.PRE_TestName3', 'TestSuite1.TestName3'],
        ]
        self.assertListEqual(actual_retry, expected_retry)

    @mock.patch(
        'pylib.utils.code_coverage_utils.PullAndMaybeMergeClangCoverageFiles'
    )
    @mock.patch('os.path.isdir', return_value=True)
    def test_ApkDelegate_Run_coverage_force_main_user(
        self, _mock_isdir, mock_pull_clang
    ):
        mock_env = mock.MagicMock(
            spec=local_device_environment.LocalDeviceEnvironment
        )
        mock_env.force_main_user = True
        mock_ti = mock.MagicMock(spec=gtest_test_instance.GtestTestInstance)
        mock_ti.coverage_dir = '/host/coverage/dir'
        mock_ti.suite = 'base_unittests'
        mock_ti.extras = {}
        mock_ti.wait_for_java_debugger = False
        mock_ti.package = 'com.example.test'
        mock_ti.runner = 'TestRunner'
        mock_ti.permissions = []
        mock_ti.use_existing_test_data = False
        mock_ti.additional_apks = []
        mock_ti.apk_helper = None
        mock_ti.test_apk_incremental_install_json = None

        delegate = local_device_gtest_run._ApkDelegate(mock_ti, mock_env)

        device = mock.MagicMock()
        device.build_version_sdk = 34
        device.GetExternalStoragePath.return_value = '/sdcard'
        device.GetAppWritablePath.return_value = '/data/data/com.example.test'
        device.ResolveSpecialPath.side_effect = lambda p: (
            '/data/media/10' + p[len('/sdcard') :]
            if p.startswith('/sdcard')
            else p
        )
        device.ReadFile.return_value = 'output'

        delegate.Run(['TestSuite.test1'], device)

        mock_pull_clang.assert_called_once_with(
            device,
            '/data/media/10/chrome/test/coverage/profraw',
            '/host/coverage/dir',
            '0',
            as_root=True,
        )

    @mock.patch(
        'pylib.utils.code_coverage_utils.PullAndMaybeMergeClangCoverageFiles'
    )
    def test_ExeDelegate_Run_coverage_force_main_user(self, mock_pull_clang):
        mock_env = mock.MagicMock(
            spec=local_device_environment.LocalDeviceEnvironment
        )
        mock_env.force_main_user = True
        mock_ti = mock.MagicMock(spec=gtest_test_instance.GtestTestInstance)
        mock_ti.coverage_dir = '/host/coverage/dir'
        mock_ti.suite = 'base_unittests'
        mock_ti.exe_dist_dir = '/path/to/test_exe__dist'

        mock_tr = mock.MagicMock()
        delegate = local_device_gtest_run._ExeDelegate(
            mock_tr, mock_ti, mock_env
        )

        device = mock.MagicMock()
        device.GetExternalStoragePath.return_value = '/sdcard'
        device.ResolveSpecialPath.side_effect = lambda p: (
            '/data/media/10' + p[len('/sdcard') :]
            if p.startswith('/sdcard')
            else p
        )

        delegate.Run(['TestSuite.test1'], device)

        mock_pull_clang.assert_called_once_with(
            device,
            '/data/media/10/chrome/test/coverage/profraw',
            '/host/coverage/dir',
            '0',
            as_root=True,
        )


if __name__ == '__main__':
    unittest.main(verbosity=2)
