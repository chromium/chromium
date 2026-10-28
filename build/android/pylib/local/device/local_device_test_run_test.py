#!/usr/bin/env vpython3
# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


import argparse
import os
import random
import unittest

import sys

sys.path.append(
    os.path.abspath(os.path.join(os.path.dirname(__file__), '../../..'))
)

from pylib.base import base_test_result
from pylib.base import test_exception
from pylib.local.device import local_device_environment
from pylib.local.device import local_device_test_run

import mock


class TestLocalDeviceTestRun(local_device_test_run.LocalDeviceTestRun):
    def __init__(self):
        env = mock.MagicMock()
        env.sharding_strategy = (
            local_device_test_run.SHARDING_STRATEGY_BOUNDED_LOAD
        )
        super().__init__(env, mock.MagicMock())


class TestLocalDeviceNonStringTestRun(local_device_test_run.LocalDeviceTestRun):
    def __init__(self):
        env = mock.MagicMock()
        env.sharding_strategy = (
            local_device_test_run.SHARDING_STRATEGY_BOUNDED_LOAD
        )
        super().__init__(env, mock.MagicMock())

    def _GetUniqueTestName(self, test):
        return test['name']


class TestLocalDeviceRetryTestRun(local_device_test_run.LocalDeviceTestRun):
    def __init__(self, tests=None):
        mock_env = mock.MagicMock()
        mock_prop_current_try = mock.PropertyMock(
            side_effect=[0, 0, 1, 1, 2, 2, 3]
        )
        type(mock_env).current_try = mock_prop_current_try
        mock_prop_max_tries = mock.PropertyMock(return_value=3)
        type(mock_env).max_tries = mock_prop_max_tries
        super().__init__(mock_env, mock.MagicMock())
        self._tests = tests or []

    def _GetTests(self):
        return self._tests

    def _GetTestsToRetry(self, tests, try_results):
        return [test for test in self._tests if test.endswith('fail')]

    def _ShouldShardTestsForDevices(self):
        pass


class LocalDeviceTestRunTest(unittest.TestCase):
    def testGetTestsToRetry_allTestsPassed(self):
        results = [
            base_test_result.BaseTestResult(
                'Test1', base_test_result.ResultType.PASS
            ),
            base_test_result.BaseTestResult(
                'Test2', base_test_result.ResultType.PASS
            ),
        ]

        tests = [r.GetName() for r in results]
        try_results = base_test_result.TestRunResults()
        try_results.AddResults(results)

        test_run = TestLocalDeviceTestRun()
        tests_to_retry = test_run._GetTestsToRetry(tests, try_results)
        self.assertEqual(0, len(tests_to_retry))

    def testGetTestsToRetry_testFailed(self):
        results = [
            base_test_result.BaseTestResult(
                'Test1', base_test_result.ResultType.FAIL
            ),
            base_test_result.BaseTestResult(
                'Test2', base_test_result.ResultType.PASS
            ),
        ]

        tests = [r.GetName() for r in results]
        try_results = base_test_result.TestRunResults()
        try_results.AddResults(results)

        test_run = TestLocalDeviceTestRun()
        tests_to_retry = test_run._GetTestsToRetry(tests, try_results)
        self.assertEqual(1, len(tests_to_retry))
        self.assertIn('Test1', tests_to_retry)

    def testGetTestsToRetry_testUnknown(self):
        results = [
            base_test_result.BaseTestResult(
                'Test2', base_test_result.ResultType.PASS
            ),
        ]

        tests = ['Test1'] + [r.GetName() for r in results]
        try_results = base_test_result.TestRunResults()
        try_results.AddResults(results)

        test_run = TestLocalDeviceTestRun()
        tests_to_retry = test_run._GetTestsToRetry(tests, try_results)
        self.assertEqual(1, len(tests_to_retry))
        self.assertIn('Test1', tests_to_retry)

    def testGetTestsToRetry_wildcardFilter_allPass(self):
        results = [
            base_test_result.BaseTestResult(
                'TestCase.Test1', base_test_result.ResultType.PASS
            ),
            base_test_result.BaseTestResult(
                'TestCase.Test2', base_test_result.ResultType.PASS
            ),
        ]

        tests = ['TestCase.*']
        try_results = base_test_result.TestRunResults()
        try_results.AddResults(results)

        test_run = TestLocalDeviceTestRun()
        tests_to_retry = test_run._GetTestsToRetry(tests, try_results)
        self.assertEqual(0, len(tests_to_retry))

    def testGetTestsToRetry_wildcardFilter_oneFails(self):
        results = [
            base_test_result.BaseTestResult(
                'TestCase.Test1', base_test_result.ResultType.PASS
            ),
            base_test_result.BaseTestResult(
                'TestCase.Test2', base_test_result.ResultType.FAIL
            ),
        ]

        tests = ['TestCase.*']
        try_results = base_test_result.TestRunResults()
        try_results.AddResults(results)

        test_run = TestLocalDeviceTestRun()
        tests_to_retry = test_run._GetTestsToRetry(tests, try_results)
        self.assertEqual(1, len(tests_to_retry))
        self.assertIn('TestCase.*', tests_to_retry)

    def testGetTestsToRetry_nonStringTests(self):
        results = [
            base_test_result.BaseTestResult(
                'TestCase.Test1', base_test_result.ResultType.PASS
            ),
            base_test_result.BaseTestResult(
                'TestCase.Test2', base_test_result.ResultType.FAIL
            ),
        ]

        tests = [
            {'name': 'TestCase.Test1'},
            {'name': 'TestCase.Test2'},
        ]
        try_results = base_test_result.TestRunResults()
        try_results.AddResults(results)

        test_run = TestLocalDeviceNonStringTestRun()
        tests_to_retry = test_run._GetTestsToRetry(tests, try_results)
        self.assertEqual(1, len(tests_to_retry))
        self.assertIsInstance(tests_to_retry[0], dict)
        self.assertEqual(tests[1], tests_to_retry[0])

    def testDeviceRecovery_highFailedTests(self):
        total_count = 10
        failed_count = (
            total_count * local_device_test_run.FAILED_TEST_PCT_MAX // 100
        ) + 1
        tests = ['TestCase.Test%d_fail' % i for i in range(1, failed_count + 1)]
        tests += [
            'TestCase.Test%d_pass' % i
            for i in range(failed_count + 1, total_count + 1)
        ]

        test_run = TestLocalDeviceRetryTestRun(tests=tests)
        with mock.patch.object(test_run, '_RecoverDevices') as mock_recover:
            test_run.RunTests(results=[])
            self.assertEqual(mock_recover.call_count, 2)

    def testDeviceRecovery_lowFailedTests(self):
        total_count = 10
        failed_count = (
            total_count * local_device_test_run.FAILED_TEST_PCT_MAX // 100
        )
        tests = ['TestCase.Test%d_fail' % i for i in range(1, failed_count + 1)]
        tests += [
            'TestCase.Test%d_pass' % i
            for i in range(failed_count + 1, total_count + 1)
        ]

        test_run = TestLocalDeviceRetryTestRun(tests=tests)
        with mock.patch.object(test_run, '_RecoverDevices') as mock_recover:
            test_run.RunTests(results=[])
            # Only called once for the last attempt.
            self.assertEqual(mock_recover.call_count, 1)

    class _DeterministicWeightTestRun(local_device_test_run.LocalDeviceTestRun):
        def __init__(self):
            env = mock.MagicMock()
            env.sharding_strategy = (
                local_device_test_run.SHARDING_STRATEGY_BOUNDED_LOAD
            )
            super().__init__(env, mock.MagicMock())

        def _EstimateTestGroupRuntime(self, test):
            return 1.0 + (self._DeterministicHash(test) % 7)

    def testApplyExternalSharding_partitionIsCompleteAndOrderIndependent(self):
        test_run = self._DeterministicWeightTestRun()
        tests = [f'Test{i}' for i in range(200)]
        for total_shards in (1, 3, 7):
            shards = []
            for i in range(total_shards):
                shards.append(
                    test_run._ApplyExternalSharding(tests, i, total_shards)
                )

            all_tests = [t for s in shards for t in s]
            self.assertEqual(len(all_tests), len(tests))
            self.assertEqual(set(all_tests), set(tests))

            shuffled = list(tests)
            random.Random(0).shuffle(shuffled)
            shuffled_shards = []
            for i in range(total_shards):
                shuffled_shards.append(
                    test_run._ApplyExternalSharding(shuffled, i, total_shards)
                )

            for i in range(total_shards):
                self.assertEqual(shards[i], shuffled_shards[i])

    def testApplyExternalSharding_boundedLoadRespectsCap(self):
        test_run = TestLocalDeviceTestRun()
        test_run._EstimateTestGroupRuntime = lambda t: (
            10.0 if t.startswith('Heavy') else 1.0
        )

        tests = [f'Light{i}' for i in range(100)]
        total_shards = 4

        # Heavy tests that hash to the same shard
        preferred = test_run._DeterministicHash('Heavy1') % total_shards
        heavy_tests = []
        for i in range(200):
            name = f'HeavyX{i}'
            if test_run._DeterministicHash(name) % total_shards == preferred:
                heavy_tests.append(name)
            if len(heavy_tests) == 4:
                break
        tests.extend(heavy_tests)

        hash_test_run = TestLocalDeviceTestRun()
        hash_test_run._env.sharding_strategy = 'hash'
        hash_test_run._EstimateTestGroupRuntime = (
            test_run._EstimateTestGroupRuntime
        )

        hash_loads = []
        bounded_loads = []
        for i in range(total_shards):
            hash_shard = hash_test_run._ApplyExternalSharding(
                tests, i, total_shards
            )
            bounded_shard = test_run._ApplyExternalSharding(
                tests, i, total_shards
            )
            hash_loads.append(
                sum(
                    hash_test_run._EstimateTestGroupRuntime(t)
                    for t in hash_shard
                )
            )
            bounded_loads.append(
                sum(
                    test_run._EstimateTestGroupRuntime(t) for t in bounded_shard
                )
            )

        weights = [test_run._EstimateTestGroupRuntime(t) for t in tests]
        cap = (
            (1 + local_device_test_run.BOUNDED_LOAD_EPSILON)
            * sum(weights)
            / total_shards
        )

        self.assertLess(max(bounded_loads), max(hash_loads))
        for load in bounded_loads:
            self.assertLessEqual(load, cap)

    def testApplyExternalSharding_boundedLoadIsStableWhenTestsChange(self):
        test_run = self._DeterministicWeightTestRun()
        tests = [f'Test{i}' for i in range(200)]
        total_shards = 4

        original_assignments = {}
        for i in range(total_shards):
            for t in test_run._ApplyExternalSharding(tests, i, total_shards):
                original_assignments[t] = i

        tests.remove('Test10')
        new_assignments = {}
        for i in range(total_shards):
            for t in test_run._ApplyExternalSharding(tests, i, total_shards):
                new_assignments[t] = i

        kept_shard = 0
        for t in tests:
            if original_assignments[t] == new_assignments[t]:
                kept_shard += 1

        self.assertGreaterEqual(kept_shard / len(tests), 0.90)

    def testApplyExternalSharding_hashStrategyMatchesLegacyAssignment(self):
        test_run = TestLocalDeviceTestRun()
        test_run._env.sharding_strategy = 'hash'
        tests = [f'Test{i}' for i in range(200)]
        total_shards = 4

        sorted_tests = sorted(
            tests, key=lambda t: (test_run._DeterministicHash(t), t)
        )
        for i in range(total_shards):
            shard = test_run._ApplyExternalSharding(tests, i, total_shards)
            expected = [
                t
                for t in sorted_tests
                if test_run._DeterministicHash(t) % total_shards == i
            ]
            self.assertEqual(shard, expected)

    def testApplyExternalSharding_invalidSettings(self):
        test_run = TestLocalDeviceTestRun()
        with self.assertRaises(test_exception.InvalidShardingSettings):
            test_run._ApplyExternalSharding(['a', 'b'], -1, 2)
        with self.assertRaises(test_exception.InvalidShardingSettings):
            test_run._ApplyExternalSharding(['a', 'b'], 2, 2)
        with self.assertRaises(test_exception.InvalidShardingSettings):
            test_run._ApplyExternalSharding(['a', 'b'], 0, 0)
        with self.assertRaises(test_exception.InvalidShardingSettings):
            test_run._ApplyExternalSharding(['a', 'b'], 0, -1)

    def testCompilePackages(self):
        device = mock.MagicMock()
        local_device_test_run.CompilePackages(
            ['org.chromium.test'], device, num_tests=20
        )
        device.RunShellCommand.assert_not_called()

        local_device_test_run.CompilePackages(
            ['org.chromium.test', 'org.chromium.test', 'org.chromium.app'],
            device,
            num_tests=21,
        )
        self.assertEqual(device.RunShellCommand.call_count, 2)
        self.assertEqual(
            device.RunShellCommand.call_args_list[0][0][0],
            ['cmd', 'package', 'compile', '-m', 'speed', 'org.chromium.test'],
        )

    @mock.patch('devil.android.sdk.adb_wrapper.AdbWrapper.GetAdbPath')
    @mock.patch('pylib.constants.GetOutDirectory', return_value='/tmp/out')
    @mock.patch('devil_chromium.Initialize')
    def testLocalDeviceEnvironment_shardingStrategy(
        self, _mock_init, _mock_out_dir, mock_adb_path
    ):
        mock_adb_path.return_value = ''
        args = argparse.Namespace(
            denylist_file=None,
            test_devices=[],
            enable_concurrent_adb=False,
            enable_device_cache=False,
            logcat_output_dir=None,
            logcat_output_file=None,
            num_retries=0,
            recover_devices=False,
            skip_clear_data=False,
            disable_persistent_shell=True,
            adb_path=None,
        )
        self.assertFalse(hasattr(args, 'sharding_strategy'))
        env = local_device_environment.LocalDeviceEnvironment(args, None, None)
        self.assertEqual(env.sharding_strategy, 'bounded-load')

        args.sharding_strategy = 'hash'
        env = local_device_environment.LocalDeviceEnvironment(args, None, None)
        self.assertEqual(env.sharding_strategy, 'hash')

    def _RecoverDevices(self, force_main_user):
        device = mock.MagicMock()
        test_run = TestLocalDeviceTestRun()
        test_run._env.force_main_user = force_main_user
        test_run._env.parallel_devices.pMap.side_effect = lambda f: f(device)
        with mock.patch.object(
            local_device_test_run.device_recovery, 'RecoverDevice'
        ) as mock_recover:
            test_run._RecoverDevices()
        mock_recover.assert_called_once_with(device, None)
        return device

    def testRecoverDevices_restoresMainUser(self):
        device = self._RecoverDevices(force_main_user=True)
        device.EnsureMainUser.assert_called_once_with()

    def testRecoverDevices_noForceMainUser(self):
        device = self._RecoverDevices(force_main_user=False)
        device.EnsureMainUser.assert_not_called()

    @mock.patch('devil.utils.timeout_retry.time.sleep')
    def testWaitUntilDeviceReady_waitsForUserUnlockAndStorage(
        self, _mock_sleep
    ):
        device = mock.MagicMock()
        device.target_user = 0
        device.build_version_sdk = 34
        device.GetAppWritablePath.return_value = '/sdcard/Download'
        device.RunShellCommand.side_effect = [
            # 1st attempt: system_server still in early boot phase (BOOTING)
            ['BOOTING'],
            # 2nd attempt: user unlocked, but /sdcard/Download FUSE not mounted
            # yet.
            ['RUNNING_UNLOCKED'],
            local_device_test_run.device_errors.AdbShellCommandFailedError(
                ['test', '-d', '/sdcard/Download'], '', 1, 'device_1'
            ),
            # 3rd attempt: user unlocked and /sdcard/Download mounted
            ['RUNNING_UNLOCKED'],
            [],
            # SetAppCompatibilityFlagsIfNecessary
            [],
            [],
        ]

        local_device_test_run.WaitUntilDeviceReady(
            device, force_main_user=False, packages=['org.chromium.test']
        )

        device.ClearCache.assert_called_once_with()
        device.WaitUntilFullyBooted.assert_called_once_with()
        device.EnsureMainUser.assert_not_called()
        self.assertEqual(device.RunShellCommand.call_count, 7)

    @mock.patch('devil.utils.timeout_retry.time.sleep')
    def testWaitUntilDeviceReady_forceMainUser(self, _mock_sleep):
        device = mock.MagicMock()
        device.target_user = None
        device.build_version_sdk = 34
        device.GetAppWritablePath.return_value = '/sdcard/Download'

        ensure_attempts = {'count': 0}

        def ensure_main_user_side_effect():
            ensure_attempts['count'] += 1
            if ensure_attempts['count'] == 1:
                raise local_device_test_run.device_errors.CommandFailedError(
                    'system_server still booting', 'device_1'
                )
            device.target_user = 10

        device.EnsureMainUser.side_effect = ensure_main_user_side_effect
        device.RunShellCommand.side_effect = [
            local_device_test_run.device_errors.CommandTimeoutError(
                'am hung mid-boot'
            ),
            ['RUNNING_UNLOCKED'],
            [],
        ]

        local_device_test_run.WaitUntilDeviceReady(
            device, force_main_user=True, packages=[]
        )

        device.ClearCache.assert_called_once_with()
        device.WaitUntilFullyBooted.assert_called_once_with()
        self.assertEqual(device.EnsureMainUser.call_count, 2)
        device.ResolveSpecialPath.assert_not_called()
        self.assertEqual(
            device.RunShellCommand.call_args_list,
            [
                mock.call(
                    ['am', 'get-started-user-state', '10'],
                    check_return=False,
                    timeout=5,
                    retries=0,
                ),
                mock.call(
                    ['am', 'get-started-user-state', '10'],
                    check_return=False,
                    timeout=5,
                    retries=0,
                ),
                mock.call(
                    ['test', '-d', '/mnt/user/10/emulated/10/Download'],
                    check_return=True,
                    as_root=True,
                    timeout=5,
                    retries=0,
                ),
            ],
        )

    @mock.patch.object(local_device_test_run, '_DEVICE_READY_MAX_TRIES', 2)
    @mock.patch('devil.utils.timeout_retry.time.sleep')
    def testWaitUntilDeviceReady_timeoutRaisesCommandTimeoutError(
        self, _mock_sleep
    ):
        device = mock.MagicMock()
        device.target_user = 0
        device.GetAppWritablePath.return_value = '/sdcard/Download'
        device.RunShellCommand.return_value = ['BOOTING']

        with self.assertRaises(
            local_device_test_run.device_errors.CommandTimeoutError
        ):
            local_device_test_run.WaitUntilDeviceReady(
                device, force_main_user=False
            )

    @mock.patch.object(local_device_test_run, 'WaitUntilDeviceReady')
    def testRunTestsOnDevice_waitsUntilDeviceReadyAfterBaseError(
        self, mock_wait_ready
    ):
        test_run = TestLocalDeviceTestRun()
        test_run._env.force_main_user = False
        test_run._installed_packages = ['org.chromium.test']
        device = mock.MagicMock()
        device.build_version_sdk = 29

        pass_result = base_test_result.BaseTestResult(
            'Test1', base_test_result.ResultType.PASS
        )
        test_run._RunTest = mock.MagicMock(
            side_effect=[
                local_device_test_run.device_errors.AdbCommandFailedError(
                    ['pull', '/sdcard/Download/out'],
                    'Input/output error',
                    1,
                    'device_1',
                ),
                (pass_result, None),
            ]
        )

        tc = local_device_test_run.test_collection.TestCollection(['Test1'])
        results = base_test_result.TestRunResults()
        exit_now = local_device_test_run.threading.Event()

        test_run._RunTestsOnDevice(device, tc, results, exit_now)

        mock_wait_ready.assert_called_once_with(
            device,
            force_main_user=False,
            packages=['org.chromium.test'],
        )
        self.assertEqual(len(results.GetPass()), 1)

    @mock.patch.object(local_device_test_run, 'WaitUntilDeviceReady')
    def testRunTestsOnDevice_waitsAfterTimeoutAndAbortsIfDeviceNeverUnlocks(
        self, mock_wait_ready
    ):
        test_run = TestLocalDeviceTestRun()
        test_run._env.force_main_user = False
        test_run._installed_packages = ['org.chromium.test']
        device = mock.MagicMock()
        device.build_version_sdk = 29

        mock_wait_ready.side_effect = (
            local_device_test_run._DeviceReadyTimeoutError('still locked')
        )
        test_run._RunTest = mock.MagicMock(
            side_effect=[
                local_device_test_run.device_errors.CommandTimeoutError(
                    'batch timed out'
                ),
                (
                    base_test_result.BaseTestResult(
                        'Test2', base_test_result.ResultType.PASS
                    ),
                    None,
                ),
            ]
        )

        tc = local_device_test_run.test_collection.TestCollection(
            ['Test1', 'Test2']
        )
        results = base_test_result.TestRunResults()
        exit_now = local_device_test_run.threading.Event()

        test_run._RunTestsOnDevice(device, tc, results, exit_now)

        mock_wait_ready.assert_called_once()
        # Test2 must NOT be started on the still-locked device.
        self.assertEqual(test_run._RunTest.call_count, 1)
        self.assertEqual(len(results.GetTimeout()), 1)


if __name__ == '__main__':
    unittest.main(verbosity=2)
