#!/usr/bin/env vpython3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tests for local_device_gtest_test_run."""

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


def isSliceInList(s, l):  # noqa: E741
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

        self._obj._env.sharding_strategy = 'bounded-load'
        expected_shard0 = self._obj._ApplyExternalSharding(tests, 0, 2)
        expected_shard1 = self._obj._ApplyExternalSharding(tests, 1, 2)

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

    def test_SetUp_disables_system_animations(self):
        for concurrent_adb in (False, True):
            with self.subTest(concurrent_adb=concurrent_adb):
                mock_env = mock.MagicMock(
                    spec=local_device_environment.LocalDeviceEnvironment
                )
                mock_env.concurrent_adb = concurrent_adb
                mock_env.force_main_user = False
                mock_ti = mock.MagicMock(
                    spec=gtest_test_instance.GtestTestInstance
                )
                mock_ti.use_existing_test_data = True
                mock_ti.deploy_mock_openxr_runtime = False
                mock_ti.GetDataDependencies.return_value = [[]]

                device = mock.MagicMock()
                device.serial = 'device_1'
                mock_env.parallel_devices.pMap.side_effect = lambda f, args: [
                    f(device, args[0])
                ]

                run = local_device_gtest_run.LocalDeviceGtestRun(
                    mock_env, mock_ti
                )
                run._delegate = mock.MagicMock()
                run.SetUp()

                expected_cmd = (
                    'settings put global window_animation_scale 0 && '
                    'settings put global transition_animation_scale 0 && '
                    'settings put global animator_duration_scale 0'
                )
                device.RunShellCommand.assert_called_once_with(
                    expected_cmd, shell=True, check_return=True
                )

    def test_SetUp_push_test_data_drops_caches_in_force_main_user(self):
        device = mock.MagicMock()
        device.ResolveSpecialPath.return_value = (
            '/data/media/10/chromium_tests_root'
        )
        self._obj._env.force_main_user = True
        self._obj._env.concurrent_adb = False
        self._obj._env.parallel_devices.pMap.side_effect = lambda fn, *args: fn(
            device, *args
        )
        self._obj._test_instance.use_existing_test_data = False
        self._obj._test_instance.deploy_mock_openxr_runtime = False
        self._obj._test_instance.GetDataDependencies.return_value = [
            ('/host/foo', 'foo')
        ]
        self._obj._delegate = mock.MagicMock()
        self._obj._delegate.GetTestDataRoot.return_value = (
            '/sdcard/chromium_tests_root'
        )

        self._obj.SetUp()

        device.RunShellCommand.assert_any_call(
            'sync && echo 3 > /proc/sys/vm/drop_caches',
            shell=True,
            check_return=True,
            as_root=True,
        )

    @mock.patch('lib.proto.exception_recorder.register')
    def test_SetUp_install_apk_records_and_reraises_base_error(
        self, mock_register
    ):
        cases = [
            (
                local_device_gtest_run.device_errors.CommandFailedError(
                    'pm path failed', 'device_1'
                ),
                local_device_gtest_run.test_exception.InstallationFailedError,
            ),
            (
                local_device_gtest_run.device_errors.CommandTimeoutError(
                    'install timed out'
                ),
                local_device_gtest_run.test_exception.InstallationTimeoutError,
            ),
            (
                local_device_gtest_run.base_error.BaseError('base error'),
                local_device_gtest_run.test_exception.InstallationError,
            ),
        ]
        for err, expected_wrapper_cls in cases:
            with self.subTest(err=type(err).__name__):
                mock_register.reset_mock()
                mock_env = mock.MagicMock(
                    spec=local_device_environment.LocalDeviceEnvironment
                )
                mock_env.concurrent_adb = False
                mock_env.force_main_user = False
                mock_ti = mock.MagicMock(
                    spec=gtest_test_instance.GtestTestInstance
                )
                mock_ti.use_existing_test_data = True
                mock_ti.deploy_mock_openxr_runtime = False
                mock_ti.GetDataDependencies.return_value = [[]]

                device = mock.MagicMock()
                device.serial = 'device_1'
                device.RunShellCommand.return_value = ['1.0', '1.0', '1.0']
                mock_env.parallel_devices.pMap.side_effect = lambda f, args: [
                    f(device, args[0])
                ]

                run = local_device_gtest_run.LocalDeviceGtestRun(
                    mock_env, mock_ti
                )
                run._delegate = mock.MagicMock()
                run._delegate.Install.side_effect = err

                run.SetUp()

                mock_register.assert_called_once()
                registered_exc = mock_register.call_args[0][0]
                self.assertIsInstance(registered_exc, expected_wrapper_cls)
                mock_env.DenylistDevice.assert_called_once_with(
                    device, 'individual_device_set_up'
                )

    @mock.patch('pylib.local.device.local_device_test_run.WaitUntilDeviceReady')
    def test_ApkDelegate_Run_recoversStdoutAfterSoftReboot(
        self, mock_wait_ready
    ):
        mock_env = mock.MagicMock(
            spec=local_device_environment.LocalDeviceEnvironment
        )
        mock_env.force_main_user = False
        mock_ti = mock.MagicMock(spec=gtest_test_instance.GtestTestInstance)
        mock_ti.coverage_dir = None
        mock_ti.suite = 'content_browsertests'
        mock_ti.extras = {}
        mock_ti.wait_for_java_debugger = False
        mock_ti.package = 'org.chromium.content_browsertests_apk'
        mock_ti.runner = (
            'org.chromium.native_test.NativeTestInstrumentationTestRunner'
        )
        mock_ti.permissions = []
        mock_ti.use_existing_test_data = False
        mock_ti.additional_apks = []
        mock_ti.apk_helper = None

        delegate = local_device_gtest_run._ApkDelegate(mock_ti, mock_env)

        device = mock.MagicMock()
        device.GetAppWritablePath.return_value = '/sdcard/Download'
        device.ReadFile.side_effect = [
            local_device_gtest_run.device_errors.AdbCommandFailedError(
                ['pull', '/sdcard/Download/out.gtest_out'],
                'Input/output error',
                1,
                'device_1',
            ),
            (
                '[ RUN      ] Suite.Test1\n'
                '[       OK ] Suite.Test1 (10 ms)\n'
                '[ RUN      ] Suite.CrashTest\n'
            ),
        ]

        lines = delegate.Run(['Suite.Test1', 'Suite.CrashTest'], device)

        mock_wait_ready.assert_called_once_with(
            device,
            force_main_user=False,
            packages=['org.chromium.content_browsertests_apk'],
        )
        self.assertEqual(device.ReadFile.call_count, 2)
        self.assertEqual(
            lines,
            [
                '[ RUN      ] Suite.Test1',
                '[       OK ] Suite.Test1 (10 ms)',
                '[ RUN      ] Suite.CrashTest',
            ],
        )

    @mock.patch('lib.proto.exception_recorder.register')
    @mock.patch('pylib.local.device.local_device_test_run.WaitUntilDeviceReady')
    def test_ApkDelegate_Run_reraisesTimeoutWhenStdoutMissing(
        self, mock_wait_ready, _mock_register
    ):
        mock_env = mock.MagicMock(
            spec=local_device_environment.LocalDeviceEnvironment
        )
        mock_env.force_main_user = False
        mock_ti = mock.MagicMock(spec=gtest_test_instance.GtestTestInstance)
        mock_ti.coverage_dir = None
        mock_ti.suite = 'content_browsertests'
        mock_ti.extras = {}
        mock_ti.wait_for_java_debugger = False
        mock_ti.package = 'org.chromium.content_browsertests_apk'
        mock_ti.runner = (
            'org.chromium.native_test.NativeTestInstrumentationTestRunner'
        )
        mock_ti.permissions = []
        mock_ti.use_existing_test_data = False
        mock_ti.additional_apks = []
        mock_ti.apk_helper = None

        delegate = local_device_gtest_run._ApkDelegate(mock_ti, mock_env)

        device = mock.MagicMock()
        device.GetAppWritablePath.return_value = '/sdcard/Download'
        device.StartInstrumentation.side_effect = (
            local_device_gtest_run.device_errors.CommandTimeoutError(
                'shard timed out'
            )
        )
        device.ReadFile.side_effect = (
            local_device_gtest_run.device_errors.AdbCommandFailedError(
                ['pull', '/sdcard/Download/out.gtest_out'],
                'No such file or directory',
                1,
                'device_1',
            )
        )

        with self.assertRaises(
            local_device_gtest_run.device_errors.CommandTimeoutError
        ):
            delegate.Run(['Suite.Test1'], device)

        mock_wait_ready.assert_not_called()

    @mock.patch('lib.proto.exception_recorder.register')
    @mock.patch('pylib.local.device.local_device_test_run.WaitUntilDeviceReady')
    def test_ApkDelegate_Run_propagatesWaitUntilDeviceReadyErrorsUnchanged(
        self, mock_wait_ready, mock_register
    ):
        mock_env = mock.MagicMock(
            spec=local_device_environment.LocalDeviceEnvironment
        )
        mock_env.force_main_user = False
        mock_ti = mock.MagicMock(spec=gtest_test_instance.GtestTestInstance)
        mock_ti.coverage_dir = None
        mock_ti.suite = 'content_browsertests'
        mock_ti.extras = {}
        mock_ti.wait_for_java_debugger = False
        mock_ti.package = 'org.chromium.content_browsertests_apk'
        mock_ti.runner = (
            'org.chromium.native_test.NativeTestInstrumentationTestRunner'
        )
        mock_ti.permissions = []
        mock_ti.use_existing_test_data = False
        mock_ti.additional_apks = []
        mock_ti.apk_helper = None

        delegate = local_device_gtest_run._ApkDelegate(mock_ti, mock_env)
        device = mock.MagicMock()
        device.GetAppWritablePath.return_value = '/sdcard/Download'
        device.ReadFile.side_effect = (
            local_device_gtest_run.device_errors.AdbCommandFailedError(
                ['pull', '/sdcard/Download/out.gtest_out'],
                'device offline',
                1,
                'device_1',
            )
        )
        mock_wait_ready.side_effect = (
            local_device_gtest_run.device_errors.DeviceUnreachableError(
                'device_1'
            )
        )

        with self.assertRaises(
            local_device_gtest_run.device_errors.DeviceUnreachableError
        ):
            delegate.Run(['Suite.Test1'], device)

        mock_register.assert_not_called()
        self.assertEqual(device.ReadFile.call_count, 1)


if __name__ == '__main__':
    unittest.main(verbosity=2)
