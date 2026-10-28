# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import fnmatch
import hashlib
import logging
import os
import signal

try:
    import _thread as thread
except ImportError:
    import thread
import threading

from devil import base_error
from devil.android import crash_handler
from devil.android import device_errors
from devil.android.sdk import version_codes
from devil.android.tools import device_recovery
from devil.utils import signal_handler
from devil.utils import timeout_retry
from pylib.base import base_test_result
from pylib.base import test_collection
from pylib.base import test_exception
from pylib.base import test_run
from pylib.utils import device_dependencies
from pylib.local.device import local_device_environment

from lib.proto import exception_recorder


_SIGTERM_TEST_LOG = (
    '  Suite execution terminated, probably due to swarming timeout.\n'
    '  Your test may not have run.'
)

# If the percentage of failed test exceeds this max value, the script
# will try to recover devices before next try.
FAILED_TEST_PCT_MAX = 90


class TestsTerminated(Exception):
    pass


# Rough runtime estimates (seconds) used only for shard balancing; unrelated
# to test timeouts.
ESTIMATED_RUNTIME_PER_CLASS_DEFAULT = 8.0
ESTIMATED_RUNTIME_PER_TEST_DEFAULT = 1.0

# Replaying android_browsertests (25 shards) over real durations: a lower cap
# tightens the slowest shard for every weight estimator tried, with the
# fraction of groups keeping their shard across test-list changes dropping
# only from 99% (0.05) to 98% (0.02). 0.20 regressed in the original sweep.
BOUNDED_LOAD_EPSILON = 0.02

SHARDING_STRATEGY_HASH = 'hash'
SHARDING_STRATEGY_BOUNDED_LOAD = 'bounded-load'
SHARDING_STRATEGIES = (SHARDING_STRATEGY_HASH, SHARDING_STRATEGY_BOUNDED_LOAD)


class LocalDeviceTestRun(test_run.TestRun):
    def __init__(self, env, test_instance):
        super().__init__(env, test_instance)
        # This is intended to be filled by a child class.
        self._installed_packages = []
        env.SetPreferredAbis(test_instance.GetPreferredAbis())

    @local_device_environment.handle_shard_failures
    def _RunTestsOnDevice(self, dev, tests, results, exit_now, num_tests=0):
        # This is performed here instead of during setup because restarting the
        # device clears app compatibility flags, which will happen if a device
        # needs to be recovered.
        SetAppCompatibilityFlagsIfNecessary(self._installed_packages, dev)
        CompilePackages(self._installed_packages, dev, num_tests)
        consecutive_device_errors = 0
        for test in tests:
            if not test:
                logging.warning('No tests in shard. Continuing.')
                tests.test_completed()
                continue
            if exit_now.isSet():
                thread.exit()

            result = None
            rerun = None
            try:
                result, rerun = crash_handler.RetryOnSystemCrash(
                    lambda d, t=test: self._RunTest(d, t), device=dev
                )
                consecutive_device_errors = 0
                if isinstance(result, base_test_result.BaseTestResult):
                    results.AddResult(result)
                elif isinstance(result, list):
                    results.AddResults(result)
                else:
                    raise Exception(
                        'Unexpected result type: %s' % type(result).__name__
                    )
            except _DeviceReadyTimeoutError as e:
                exception_recorder.register(e)
                raise
            except device_errors.CommandTimeoutError as e:
                exception_recorder.register(e)
                # Test timeouts don't count as device errors for the purpose
                # of bad device detection.
                consecutive_device_errors = 0

                if isinstance(test, list):
                    result_log = ''
                    if len(test) > 1:
                        result_log = (
                            'The test command timed out when running multiple '
                            'tests including this test. It does not '
                            'necessarily mean this specific test timed out.'
                        )
                        # Ensure instrumentation tests not batched at env level retries.
                        for t in test:
                            # |dict| type infers it's an instrumentation test.
                            if isinstance(t, dict) and t['annotations']:
                                t['annotations'].pop('Batch', None)

                    results.AddResults(
                        base_test_result.BaseTestResult(
                            self._GetUniqueTestName(t),
                            base_test_result.ResultType.TIMEOUT,
                            log=result_log,
                        )
                        for t in test
                    )
                else:
                    results.AddResult(
                        base_test_result.BaseTestResult(
                            self._GetUniqueTestName(test),
                            base_test_result.ResultType.TIMEOUT,
                        )
                    )
                try:
                    WaitUntilDeviceReady(
                        dev,
                        force_main_user=self._env.force_main_user,
                        packages=self._installed_packages,
                    )
                except base_error.BaseError as wait_err:
                    exception_recorder.register(wait_err)
                    raise
            except device_errors.DeviceUnreachableError as e:
                exception_recorder.register(e)
                # If the device is no longer reachable then terminate this
                # _RunTestsOnDevice call.
                raise
            except base_error.BaseError as e:
                exception_recorder.register(e)
                # If we get a device error but believe the device is still
                # reachable, attempt to continue using it.
                if isinstance(tests, test_collection.TestCollection):
                    rerun = test

                consecutive_device_errors += 1
                if consecutive_device_errors >= 3:
                    # We believe the device is still reachable and may still be usable,
                    # but if it fails repeatedly, we shouldn't attempt to keep using
                    # it.
                    logging.error(
                        'Repeated failures on device %s. Abandoning.', str(dev)
                    )
                    raise

                logging.exception(
                    'Attempting to continue using device %s despite failure (%d/3).',
                    str(dev),
                    consecutive_device_errors,
                )
                try:
                    WaitUntilDeviceReady(
                        dev,
                        force_main_user=self._env.force_main_user,
                        packages=self._installed_packages,
                    )
                except base_error.BaseError as wait_err:
                    exception_recorder.register(wait_err)
                    raise

            finally:
                if isinstance(tests, test_collection.TestCollection):
                    if rerun:
                        tests.add(rerun)
                    tests.test_completed()

        logging.info('Finished running tests on this device.')

    # override
    def RunTests(self, results, raw_logs_fh=None):
        tests = self._GetTests()
        total_test_count = len(FlattenTestList(tests))

        exit_now = threading.Event()

        def stop_tests(_signum, _frame):
            logging.critical('Received SIGTERM. Stopping test execution.')
            exit_now.set()
            raise TestsTerminated()

        try:
            with signal_handler.AddSignalHandler(signal.SIGTERM, stop_tests):
                self._env.ResetCurrentTry()
                while self._env.current_try < self._env.max_tries and tests:
                    tries = self._env.current_try
                    flatten_tests = FlattenTestList(tests)
                    logging.info(
                        'STARTING TRY #%d/%d', tries + 1, self._env.max_tries
                    )
                    if tries > 0 and self._env.recover_devices:
                        # The variable "tests" is reused to store the failed tests.
                        failed_test_pct = (
                            100 * len(flatten_tests) // total_test_count
                        )
                        if failed_test_pct > FAILED_TEST_PCT_MAX:
                            logging.info(
                                'Attempting to recover devices as the percentage of failed '
                                'tests (%d%%) exceeds the threshold %d%%.',
                                failed_test_pct,
                                FAILED_TEST_PCT_MAX,
                            )
                            self._RecoverDevices()
                        elif tries + 1 == self._env.max_tries:
                            logging.info(
                                'Attempting to recover devices prior to last test attempt.'
                            )
                            self._RecoverDevices()
                    logging.info(
                        'Will run %d tests, grouped into %d groups, on %d devices: %s',
                        len(flatten_tests),
                        len(tests),
                        len(self._env.devices),
                        ', '.join(str(d) for d in self._env.devices),
                    )
                    for t in tests:
                        logging.debug('  %s', t)

                    try_results = base_test_result.TestRunResults()
                    test_names = (
                        self._GetUniqueTestName(t) for t in flatten_tests
                    )
                    try_results.AddResults(
                        base_test_result.BaseTestResult(
                            t, base_test_result.ResultType.NOTRUN
                        )
                        for t in test_names
                        if not t.endswith('*')
                    )

                    # As soon as we know the names of the tests, we populate |results|.
                    # The tests in try_results will have their results updated by
                    # try_results.AddResult() as they are run.
                    results.append(try_results)

                    try:
                        if self._ShouldShardTestsForDevices():
                            tc = test_collection.TestCollection(
                                self._CreateShardsForDevices(tests)
                            )
                            self._env.parallel_devices.pMap(
                                self._RunTestsOnDevice,
                                tc,
                                try_results,
                                exit_now,
                                len(flatten_tests),
                            ).pGet(None)
                        else:
                            self._env.parallel_devices.pMap(
                                self._RunTestsOnDevice,
                                tests,
                                try_results,
                                exit_now,
                                len(flatten_tests),
                            ).pGet(None)
                    except TestsTerminated:
                        for unknown_result in try_results.GetUnknown():
                            try_results.AddResult(
                                base_test_result.BaseTestResult(
                                    unknown_result.GetName(),
                                    base_test_result.ResultType.TIMEOUT,
                                    log=_SIGTERM_TEST_LOG,
                                )
                            )
                        raise

                    self._env.IncrementCurrentTry()
                    tests = self._GetTestsToRetry(tests, try_results)

                    logging.info(
                        'FINISHED TRY #%d/%d', tries + 1, self._env.max_tries
                    )
                    if tests:
                        logging.info('%d failed tests remain.', len(tests))
                    else:
                        logging.info('All tests completed.')
        except TestsTerminated:
            pass

    def _RecoverDevices(self):
        def recover(device):
            device_recovery.RecoverDevice(device, None)
            # Recovery reboots the device, which brings HSUM devices back to
            # the system user with the main user stopped and its storage
            # locked ("Required key not available").
            if self._env.force_main_user:
                device.EnsureMainUser()

        self._env.parallel_devices.pMap(recover)

    def _GetTestsToRetry(self, tests, try_results):
        """Get the tests to retry.

        Note "tests" can have groups of test. For gtest, we would like to add
        the entire group to retry as they are PRE test group. For instrumentation,
        we only keep the failed tests in that group.
        """

        def is_failure_result(test_result):
            if isinstance(test_result, list):
                return any(is_failure_result(r) for r in test_result)
            return test_result is None or test_result.GetType() not in (
                base_test_result.ResultType.PASS,
                base_test_result.ResultType.SKIP,
            )

        def get_tests_to_retry(test_list, all_test_results):
            failed_tests = []
            for test in test_list:
                if isinstance(test, list):
                    failed_test = get_tests_to_retry(test, all_test_results)
                    if failed_test:
                        failed_tests.append(
                            test
                            if self._ShouldRetryFullGroup(test)
                            else failed_test
                        )
                else:
                    name = self._GetUniqueTestName(test)
                    # When specifying a test filter, names can contain trailing wildcards.
                    # See local_device_gtest_run._ExtractTestsFromFilters()
                    if name.endswith('*'):
                        result = [
                            r
                            for n, r in all_test_results.items()
                            if fnmatch.fnmatch(n, name)
                        ]
                    else:
                        result = all_test_results.get(name)
                    if is_failure_result(result) and self._ShouldRetry(
                        test, result
                    ):
                        failed_tests.append(test)
            return failed_tests

        all_test_results = {r.GetName(): r for r in try_results.GetAll()}
        return get_tests_to_retry(tests, all_test_results)

    def _ApplyExternalSharding(self, tests, shard_index, total_shards):
        logging.info(
            'Using external sharding settings. This is shard %d/%d',
            shard_index,
            total_shards,
        )

        if total_shards <= 0 or shard_index < 0 or total_shards <= shard_index:
            raise test_exception.InvalidShardingSettings(
                shard_index, total_shards
            )

        grouped_tests = self._GroupTests(tests)
        groups = [(self._GetUniqueTestName(g), g) for g in grouped_tests]
        # Placement order depends only on group names, never on other groups'
        # weights.
        groups.sort(
            key=lambda item: (self._DeterministicHash(item[0]), item[0])
        )

        # Fingerprint so divergence between shards is detectable from logs: the
        # partition is only valid if every shard computes it from the identical
        # list.
        fingerprint = hashlib.sha256(
            '\n'.join(name for name, _ in groups).encode('utf-8')
        ).hexdigest()[:12]
        strategy = self._env.sharding_strategy
        logging.info(
            'Sharding %d test groups with strategy %s (input fingerprint %s)',
            len(groups),
            strategy,
            fingerprint,
        )

        if strategy == SHARDING_STRATEGY_HASH:
            return [
                g
                for name, g in groups
                if self._DeterministicHash(name) % total_shards == shard_index
            ]

        # Bounded-load hashing: each group keeps its hash shard unless that
        # would push the shard above (1 + eps) * mean estimated load, in which
        # case it spills to the next shard with room. Only a small fraction of
        # groups move when the test list changes, unlike LPT where a group's
        # shard depends on every other group's weight.
        weights = [self._EstimateTestGroupRuntime(g) for _, g in groups]
        cap = (1 + BOUNDED_LOAD_EPSILON) * sum(weights) / total_shards
        load = [0.0] * total_shards
        shard_assigned = [[] for _ in range(total_shards)]

        for (name, group), weight in zip(groups, weights):
            preferred = self._DeterministicHash(name) % total_shards
            for k in range(total_shards):
                shard = (preferred + k) % total_shards
                if load[shard] + weight <= cap or k == total_shards - 1:
                    break
            shard_assigned[shard].append(group)
            load[shard] += weight

        return shard_assigned[shard_index]

    def _EstimateTestGroupRuntime(self, test):
        """Returns a rough estimated runtime (seconds) for balancing shards.

        This is NOT the test timeout; the numbers are coarse heuristics, not
        measurements.
        """
        tests_in_group = test if isinstance(test, (list, tuple)) else [test]
        assert tests_in_group, 'Empty test group'
        return (
            ESTIMATED_RUNTIME_PER_CLASS_DEFAULT
            + len(tests_in_group) * ESTIMATED_RUNTIME_PER_TEST_DEFAULT
        )

    def _DeterministicHash(self, test_name):
        """Return the deterministic hash for a test name, as an integer."""
        assert isinstance(test_name, str), 'Expecting a string.'
        hash_bytes = hashlib.sha256(test_name.encode('utf-8')).digest()
        # To speed thing up, only take the last 3 bytes
        return int.from_bytes(hash_bytes[-3:], byteorder='big')

    def _CreateShardsForDevices(self, tests):
        raise NotImplementedError

    def _GetUniqueTestName(self, test):
        """Returns a string identifier for a single test.

        Args:
          test: A single test identifier. The base implementation only handles
            str tests (e.g. gtests) and returns it unchanged; subclasses such as
            LocalDeviceInstrumentationTestRun override this for dict tests.
            Cannot be a test group or batch list.
        """
        return test

    def _ShouldRetry(self, test, result):
        return True

    def _ShouldRetryFullGroup(self, test_group):
        """Whether should retry the full group if the group has test failure."""
        return False

    # override
    def GetTestsForListing(self):
        ret = self._GetTests()
        ret = FlattenTestList(ret)
        ret.sort()
        return ret

    def GetDataDepsForListing(self):
        device_root = '$CHROMIUM_TESTS_ROOT'
        host_device_tuples = self._test_instance.GetDataDependencies()
        host_device_tuples = device_dependencies.SubstituteDeviceRoot(
            host_device_tuples, device_root
        )
        host_device_tuples = device_dependencies.ExpandDataDependencies(
            host_device_tuples
        )

        return sorted(
            host_device_tuples, key=lambda x: (x[1], os.path.relpath(x[0]))
        )

    def _GetTests(self):
        """Get the tests to run on the assigned shard index.

        Shall be implemented by the subclasses.
        """
        raise NotImplementedError

    def _GroupTests(self, tests):
        """Group tests by tests that should run in the same test invocation.

        Can be override by subclasses if needed. Examples are:
          - gtest: PRE_ tests
          - instrumentation test: unit tests and batched tests.

        Args:
          tests: a flatten list of tests. The element can be a string for gtest,
            or a dict for instrumentation tests.

        Return a list whose element can be a test, or a list of tests.
        """
        return tests

    def _RunTest(self, device, test):
        raise NotImplementedError

    def _ShouldShardTestsForDevices(self):
        raise NotImplementedError


def FlattenTestList(values):
    """Returns a list with all nested lists (shard groupings) expanded."""
    ret = []
    for v in values:
        if isinstance(v, list):
            ret += v
        else:
            ret.append(v)
    return ret


_DEVICE_READY_MAX_TRIES = 60
_DEVICE_READY_WAIT_PERIOD = 1
_DEVICE_READY_SHELL_TIMEOUT = 5


class _DeviceReadyTimeoutError(device_errors.CommandTimeoutError):
    """Raised when WaitUntilDeviceReady times out waiting for unlock/storage."""


def WaitUntilDeviceReady(device, force_main_user=False, packages=None):
    """Waits for a device to be fully booted, unlocked, and storage-mounted.

    During a userspace soft-reboot (e.g. when surfaceflinger or system_server
    crashes and restarts zygote), adbd stays alive and sys.boot_completed
    remains '1', so WaitUntilFullyBooted() alone can return while system_server
    is still in an early boot phase with the target user locked and /sdcard
    FUSE unmounted (via `vdc volume abort_fuse`). Starting instrumentation in
    that window causes ActivityManagerService.startInstrumentation() to fail
    internally and force-stop the package without notifying the
    InstrumentationWatcher, hanging `am instrument -w` until the shard timeout
    expires.
    """
    device.ClearCache()
    device.WaitUntilFullyBooted()

    main_user_ensured = not force_main_user
    last_state = {'user_id': 0, 'storage_path': None}

    def is_user_unlocked_and_storage_ready():
        nonlocal main_user_ensured
        try:
            if not main_user_ensured:
                device.EnsureMainUser()
                if not isinstance(getattr(device, 'target_user', None), int):
                    return False
                main_user_ensured = True

            user_id = (
                device.target_user
                if isinstance(getattr(device, 'target_user', None), int)
                else 0
            )
            # On HSUM (force_main_user with non-zero user), ResolveSpecialPath()
            # maps /sdcard to the lower /data/media/<user> directory, which
            # remains present even while FUSE is unmounted. Probe the user's
            # actual FUSE mount namespace instead.
            storage_path = (
                f'/mnt/user/{user_id}/emulated/{user_id}/Download'
                if force_main_user and user_id != 0
                else device.GetAppWritablePath()
            )
            last_state['user_id'] = user_id
            last_state['storage_path'] = storage_path

            output = device.RunShellCommand(
                ['am', 'get-started-user-state', str(user_id)],
                check_return=False,
                timeout=_DEVICE_READY_SHELL_TIMEOUT,
                retries=0,
            )
            if not any('RUNNING_UNLOCKED' in line for line in output):
                return False
            device.RunShellCommand(
                ['test', '-d', storage_path],
                check_return=True,
                as_root=force_main_user,
                timeout=_DEVICE_READY_SHELL_TIMEOUT,
                retries=0,
            )
            return True
        except (
            device_errors.CommandFailedError,
            device_errors.CommandTimeoutError,
        ):
            return False

    if not timeout_retry.WaitFor(
        is_user_unlocked_and_storage_ready,
        wait_period=_DEVICE_READY_WAIT_PERIOD,
        max_tries=_DEVICE_READY_MAX_TRIES,
    ):
        raise _DeviceReadyTimeoutError(
            f'Timed out waiting for device {device} user '
            f'{last_state["user_id"]} unlock and external storage '
            f'({last_state["storage_path"]}).'
        )
    if packages:
        SetAppCompatibilityFlagsIfNecessary(packages, device)


def SetAppCompatibilityFlagsIfNecessary(packages, device):
    """Sets app compatibility flags on the given packages and device.

    Args:
      packages: A list of strings containing package names to apply flags to.
      device: A DeviceUtils instance to apply the flags on.
    """

    def set_flag_for_packages(flag, enable):
        enable_str = 'enable' if enable else 'disable'
        for p in packages:
            cmd = ['am', 'compat', enable_str, flag, p]
            device.RunShellCommand(cmd)

    sdk_version = device.build_version_sdk
    if sdk_version >= version_codes.R:
        # These flags are necessary to use the legacy storage permissions on R+.
        # See crbug.com/1173699 for more information.
        set_flag_for_packages('DEFAULT_SCOPED_STORAGE', False)
        set_flag_for_packages('FORCE_ENABLE_SCOPED_STORAGE', False)


def CompilePackages(packages, device, num_tests):
    """Ahead-of-time compiles test APKs with ART when worthwhile."""
    if num_tests <= 20:
        return
    for p in dict.fromkeys(packages):
        device.RunShellCommand(
            ['cmd', 'package', 'compile', '-m', 'speed', p],
            check_return=False,
            timeout=180,
            retries=0,
        )


class NoTestsError(Exception):
    """Error for when no tests are found."""
