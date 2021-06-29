# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import fnmatch
import logging
import posixpath
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
from pylib import valgrind_tools
from pylib.base import base_test_result
from pylib.base import test_run
from pylib.base import test_collection
from pylib.local.device import local_device_environment


_SIGTERM_TEST_LOG = (
  '  Suite execution terminated, probably due to swarming timeout.\n'
  '  Your test may not have run.')


def SubstituteDeviceRoot(device_path, device_root):
  if not device_path:
    return device_root
  elif isinstance(device_path, list):
    return posixpath.join(*(p if p else device_root for p in device_path))
  else:
    return device_path


class TestsTerminated(Exception):
  pass


class InvalidShardingSettings(Exception):
  def __init__(self, shard_index, total_shards):
    super(InvalidShardingSettings, self).__init__(
        'Invalid sharding settings. shard_index: %d total_shards: %d'
            % (shard_index, total_shards))


class LocalDeviceTestRun(test_run.TestRun):

  def __init__(self, env, test_instance):
    super(LocalDeviceTestRun, self).__init__(env, test_instance)
    self._tools = {}
    # This is intended to be filled by a child class.
    self._installed_packages = []
    env.SetPreferredAbis(test_instance.GetPreferredAbis())

  #override
  def RunTests(self, results):
    tests = self._GetTests()

    exit_now = threading.Event()

    @local_device_environment.handle_shard_failures
    def run_tests_on_device(dev, tests, results):
      # This is performed here instead of during setup because restarting the
      # device clears app compatibility flags, which will happen if a device
      # needs to be recovered.
      SetAppCompatibilityFlagsIfNecessary(self._installed_packages, dev)
      consecutive_device_errors = 0
      for test in tests:
        if not test:
          logging.warning('No tests in shared. Continuing.')
          tests.test_completed()
          continue
        if exit_now.isSet():
          thread.exit()

        result = None
        rerun = None
        try:
          result, rerun = crash_handler.RetryOnSystemCrash(
              lambda d, t=test: self._RunTest(d, t),
              device=dev)
          consecutive_device_errors = 0
          if isinstance(result, base_test_result.BaseTestResult):
            results.AddResult(result)
          elif isinstance(result, list):
            results.AddResults(result)
          else:
            raise Exception(
                'Unexpected result type: %s' % type(result).__name__)
        except device_errors.CommandTimeoutError:
          # Test timeouts don't count as device errors for the purpose
          # of bad device detection.
          consecutive_device_errors = 0

          if isinstance(test, list):
            results.AddResults(
                base_test_result.BaseTestResult(
                    self._GetUniqueTestName(t),
                    base_test_result.ResultType.TIMEOUT) for t in test)
          else:
            results.AddResult(
                base_test_result.BaseTestResult(
                    self._GetUniqueTestName(test),
                    base_test_result.ResultType.TIMEOUT))
        except Exception as e:  # pylint: disable=broad-except
          if isinstance(tests, test_collection.TestCollection):
            rerun = test
          if (isinstance(e, device_errors.DeviceUnreachableError)
              or not isinstance(e, base_error.BaseError)):
            # If we get a device error but believe the device is still
            # reachable, attempt to continue using it. Otherwise, raise
            # the exception and terminate this run_tests_on_device call.
            raise

          consecutive_device_errors += 1
          if consecutive_device_errors >= 3:
            # We believe the device is still reachable and may still be usable,
            # but if it fails repeatedly, we shouldn't attempt to keep using
            # it.
            logging.error('Repeated failures on device %s. Abandoning.',
                          str(dev))
            raise

          logging.exception(
              'Attempting to continue using device %s despite failure (%d/3).',
              str(dev), consecutive_device_errors)

        finally:
          if isinstance(tests, test_collection.TestCollection):
            if rerun:
              tests.add(rerun)
            tests.test_completed()

      logging.info('Finished running tests on this device.')

    def stop_tests(_signum, _frame):
      logging.critical('Received SIGTERM. Stopping test execution.')
      exit_now.set()
      raise TestsTerminated()

    try:
      with signal_handler.AddSignalHandler(signal.SIGTERM, stop_tests):
        self._env.ResetCurrentTry()
        while self._env.current_try < self._env.max_tries and tests:
          tries = self._env.current_try
          grouped_tests = self._GroupTests(tests)
          logging.info('STARTING TRY #%d/%d', tries + 1, self._env.max_tries)
          if tries > 0 and self._env.recover_devices:
            if any(d.build_version_sdk == version_codes.LOLLIPOP_MR1
                   for d in self._env.devices):
              logging.info(
                  'Attempting to recover devices due to known issue on L MR1. '
                  'See crbug.com/787056 for details.')
              self._env.parallel_devices.pMap(
                  device_recovery.RecoverDevice, None)
            elif tries + 1 == self._env.max_tries:
              logging.info(
                  'Attempting to recover devices prior to last test attempt.')
              self._env.parallel_devices.pMap(
                  device_recovery.RecoverDevice, None)
          logging.info('Will run %d tests on %d devices: %s',
                       len(tests), len(self._env.devices),
                       ', '.join(str(d) for d in self._env.devices))
          for t in tests:
            logging.debug('  %s', t)

          try_results = base_test_result.TestRunResults()
          test_names = (self._GetUniqueTestName(t) for t in tests)
          try_results.AddResults(
              base_test_result.BaseTestResult(
                  t, base_test_result.ResultType.NOTRUN)
              for t in test_names if not t.endswith('*'))

          # As soon as we know the names of the tests, we populate |results|.
          # The tests in try_results will have their results updated by
          # try_results.AddResult() as they are run.
          results.append(try_results)

          try:
            if self._ShouldShard():
              tc = test_collection.TestCollection(
                  self._CreateShards(grouped_tests))
              self._env.parallel_devices.pMap(
                  run_tests_on_device, tc, try_results).pGet(None)
            else:
              self._env.parallel_devices.pMap(run_tests_on_device,
                                              grouped_tests,
                                              try_results).pGet(None)
          except TestsTerminated:
            for unknown_result in try_results.GetUnknown():
              try_results.AddResult(
                  base_test_result.BaseTestResult(
                      unknown_result.GetName(),
                      base_test_result.ResultType.TIMEOUT,
                      log=_SIGTERM_TEST_LOG))
            raise

          self._env.IncrementCurrentTry()
          tests = self._GetTestsToRetry(tests, try_results)

          logging.info('FINISHED TRY #%d/%d', tries + 1, self._env.max_tries)
          if tests:
            logging.info('%d failed tests remain.', len(tests))
          else:
            logging.info('All tests completed.')
    except TestsTerminated:
      pass

  def _GetTestsToRetry(self, tests, try_results):

    def is_failure_result(test_result):
      if isinstance(test_result, list):
        return any(is_failure_result(r) for r in test_result)
      return (
          test_result is None
          or test_result.GetType() not in (
              base_test_result.ResultType.PASS,
              base_test_result.ResultType.SKIP))

    all_test_results = {r.GetName(): r for r in try_results.GetAll()}

    tests_and_names = ((t, self._GetUniqueTestName(t)) for t in tests)

    tests_and_results = {}
    for test, name in tests_and_names:
      if name.endswith('*'):
        tests_and_results[name] = (test, [
            r for n, r in all_test_results.items() if fnmatch.fnmatch(n, name)
        ])
      else:
        tests_and_results[name] = (test, all_test_results.get(name))

    failed_tests_and_results = ((test, result)
                                for test, result in tests_and_results.values()
                                if is_failure_result(result))

    return [t for t, r in failed_tests_and_results if self._ShouldRetry(t, r)]

  def _ApplyExternalSharding(self, tests, shard_index, total_shards):
    logging.info('Using external sharding settings. This is shard %d/%d',
                 shard_index, total_shards)

    if total_shards < 0 or shard_index < 0 or total_shards <= shard_index:
      raise InvalidShardingSettings(shard_index, total_shards)

    sharded_tests = []

    # Group tests by tests that should run in the same test invocation - either
    # unit tests or batched tests.
    grouped_tests = self._GroupTests(tests)

    # Partition grouped tests approximately evenly across shards.
    partitioned_tests = self._PartitionTests(grouped_tests, total_shards,
                                             float('inf'))
    if len(partitioned_tests) <= shard_index:
      return []
    for t in partitioned_tests[shard_index]:
      if isinstance(t, list):
        sharded_tests.extend(t)
      else:
        sharded_tests.append(t)
    return sharded_tests

  # Partition tests evenly into |num_desired_partitions| partitions where
  # possible. However, many constraints make partitioning perfectly impossible.
  # If the max_partition_size isn't large enough, extra partitions may be
  # created (infinite max size should always return precisely the desired
  # number of partitions). Even if the |max_partition_size| is technically large
  # enough to hold all of the tests in |num_desired_partitions|, we attempt to
  # keep test order relatively stable to minimize flakes, so when tests are
  # grouped (eg. batched tests), we cannot perfectly fill all paritions as that
  # would require breaking up groups.
  def _PartitionTests(self, tests, num_desired_partitions, max_partition_size):
    # pylint: disable=no-self-use
    partitions = []

    # Sort by hash so we don't put all tests in a slow suite in the same
    # partition.
    tests = sorted(
        tests,
        key=lambda t: hash(
            self._GetUniqueTestName(t[0] if isinstance(t, list) else t)))

    def CountTestsIndividually(test):
      if not isinstance(test, list):
        return False
      annotations = test[0]['annotations']
      # UnitTests tests are really fast, so to balance shards better, count
      # UnitTests Batches as single tests.
      return ('Batch' not in annotations
              or annotations['Batch']['value'] != 'UnitTests')

    num_not_yet_allocated = sum(
        [len(test) - 1 for test in tests if CountTestsIndividually(test)])
    num_not_yet_allocated += len(tests)

    # Fast linear partition approximation capped by max_partition_size. We
    # cannot round-robin or otherwise re-order tests dynamically because we want
    # test order to remain stable.
    partition_size = min(num_not_yet_allocated // num_desired_partitions,
                         max_partition_size)
    partitions.append([])
    last_partition_size = 0
    for test in tests:
      test_count = len(test) if CountTestsIndividually(test) else 1
      # Make a new shard whenever we would overfill the previous one. However,
      # if the size of the test group is larger than the max partition size on
      # its own, just put the group in its own shard instead of splitting up the
      # group.
      if (last_partition_size + test_count > partition_size
          and last_partition_size > 0):
        num_desired_partitions -= 1
        if num_desired_partitions <= 0:
          # Too many tests for number of partitions, just fill all partitions
          # beyond num_desired_partitions.
          partition_size = max_partition_size
        else:
          # Re-balance remaining partitions.
          partition_size = min(num_not_yet_allocated // num_desired_partitions,
                               max_partition_size)
        partitions.append([])
        partitions[-1].append(test)
        last_partition_size = test_count
      else:
        partitions[-1].append(test)
        last_partition_size += test_count

      num_not_yet_allocated -= test_count

    if not partitions[-1]:
      partitions.pop()
    return partitions

  def GetTool(self, device):
    if str(device) not in self._tools:
      self._tools[str(device)] = valgrind_tools.CreateTool(
          self._env.tool, device)
    return self._tools[str(device)]

  def _CreateShards(self, tests):
    raise NotImplementedError

  def _GetUniqueTestName(self, test):
    # pylint: disable=no-self-use
    return test

  def _ShouldRetry(self, test, result):
    # pylint: disable=no-self-use,unused-argument
    return True

  def _GetTests(self):
    raise NotImplementedError

  def _GroupTests(self, tests):
    # pylint: disable=no-self-use
    return tests

  def _RunTest(self, device, test):
    raise NotImplementedError

  def _ShouldShard(self):
    raise NotImplementedError


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


class NoTestsError(Exception):
  """Error for when no tests are found."""
