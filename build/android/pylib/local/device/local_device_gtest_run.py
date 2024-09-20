# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


import contextlib
import collections
import fnmatch
import itertools
import logging
import math
import os
import posixpath
import subprocess
import shutil
import time

from devil.android import crash_handler
from devil.android import device_errors
from devil.android import device_temp_file
from devil.android import logcat_monitor
from devil.android import ports
from devil.android.sdk import version_codes
from devil.utils import reraiser_thread
from incremental_install import installer
from pylib import constants
from pylib.base import base_test_result
from pylib.gtest import gtest_test_instance
from pylib.local import local_test_server_spawner
from pylib.local.device import local_device_environment
from pylib.local.device import local_device_test_run
from pylib.symbols import stack_symbolizer
from pylib.utils import code_coverage_utils
from pylib.utils import device_dependencies
from pylib.utils import google_storage_helper
from pylib.utils import logdog_helper
from py_trace_event import trace_event
from py_utils import contextlib_ext
from py_utils import tempfile_ext
import tombstones

_MAX_INLINE_FLAGS_LENGTH = 50  # Arbitrarily chosen.
_EXTRA_COMMAND_LINE_FILE = (
    'org.chromium.native_test.NativeTest.CommandLineFile')
_EXTRA_COMMAND_LINE_FLAGS = (
    'org.chromium.native_test.NativeTest.CommandLineFlags')
_EXTRA_COVERAGE_DEVICE_FILE = (
    'org.chromium.native_test.NativeTest.CoverageDeviceFile')
_EXTRA_STDOUT_FILE = (
    'org.chromium.native_test.NativeTestInstrumentationTestRunner'
        '.StdoutFile')
_EXTRA_TEST = (
    'org.chromium.native_test.NativeTestInstrumentationTestRunner'
        '.Test')
_EXTRA_TEST_LIST = (
    'org.chromium.native_test.NativeTestInstrumentationTestRunner'
        '.TestList')

# Used to identify the prefix in gtests.
_GTEST_PRETEST_PREFIX = 'PRE_'

_SECONDS_TO_NANOS = int(1e9)

# Tests that use SpawnedTestServer must run the LocalTestServerSpawner on the
# host machine.
# TODO(jbudorick): Move this up to the test instance if the net test server is
# handled outside of the APK for the remote_device environment.
_SUITE_REQUIRES_TEST_SERVER_SPAWNER = [
  'components_browsertests', 'content_unittests', 'content_browsertests',
  'net_unittests', 'services_unittests', 'unit_tests'
]

# No-op context manager. If we used Python 3, we could change this to
# contextlib.ExitStack()
class _NullContextManager:
  def __enter__(self):
    pass
  def __exit__(self, *args):
    pass


def _GenerateSequentialFileNames(filename):
  """Infinite generator of names: 'name.ext', 'name_1.ext', 'name_2.ext', ..."""
  yield filename
  base, ext = os.path.splitext(filename)
  for i in itertools.count(1):
    yield '%s_%d%s' % (base, i, ext)


def _ExtractTestsFromFilters(gtest_filters):
  """Returns the list of tests specified by the given filters.

  Returns:
    None if the device should be queried for the test list instead.
  """
  # - means exclude filter.
  for gtest_filter in gtest_filters:
    if '-' in gtest_filter:
      return None
  # Empty means all tests
  if not any(gtest_filters):
    return None

  if len(gtest_filters) == 1:
    patterns = gtest_filters[0].split(':')
    # For a single pattern, allow it even if it has a wildcard so long as the
    # wildcard comes at the end and there is at least one . to prove the scope
    # is not too large.
    # This heuristic is not necessarily faster, but normally is.
    if len(patterns) == 1 and patterns[0].endswith('*'):
      no_suffix = patterns[0].rstrip('*')
      if '*' not in no_suffix and '.' in no_suffix:
        return patterns

  all_patterns = set(gtest_filters[0].split(':'))
  for gtest_filter in gtest_filters:
    patterns = gtest_filter.split(':')
    for pattern in patterns:
      if '*' in pattern:
        return None
    all_patterns = all_patterns.intersection(set(patterns))
  return list(all_patterns)


def _GetDeviceTimeoutMultiplier():
  # Emulated devices typically run 20-150x slower than real-time.
  # Give a way to control this through the DEVICE_TIMEOUT_MULTIPLIER
  # environment variable.
  multiplier = os.getenv("DEVICE_TIMEOUT_MULTIPLIER")
  if multiplier:
    return int(multiplier)
  return 1


def _GetLLVMProfilePath(device_coverage_dir, suite, coverage_index):
  """Gets 'LLVM_PROFILE_FILE' environment variable path.

  Dumping data to ONLY 1 file may cause warning and data overwrite in
  browsertests, so that pattern "%2m" is used to expand to 2 raw profiles
  at runtime.

  Args:
    device_coverage_dir: The directory to generate data on device.
    suite: Test suite name.
    coverage_index: The incremental index for this test suite.

  Returns:
    The path pattern for environment variable 'LLVM_PROFILE_FILE'.
  """
  # "%2m" is used to expand to 2 raw profiles at runtime.
  # "%c" enables continuous mode. See crbug.com/1468343, crbug.com/1518474
  # For more details, refer to:
  #   https://clang.llvm.org/docs/SourceBasedCodeCoverage.html
  return posixpath.join(device_coverage_dir,
                        '_'.join([suite,
                                  str(coverage_index), '%2m%c.profraw']))


def _GroupPreTests(tests):
  pre_tests = dict()
  other_tests = []
  for test in tests:
    test_name_start = max(test.find('.') + 1, 0)
    test_name = test[test_name_start:]
    if test_name_start > 0 and test_name.startswith(_GTEST_PRETEST_PREFIX):
      test_suite = test[:test_name_start - 1]
      trim_test = test
      trim_tests = [test]

      while test_name.startswith(_GTEST_PRETEST_PREFIX):
        test_name = test_name[len(_GTEST_PRETEST_PREFIX):]
        trim_test = '%s.%s' % (test_suite, test_name)
        trim_tests.append(trim_test)

      # The trim test should exist at first place. For example, if a test has
      # been disabled, there is no need to run PRE_ test with this test.
      if trim_test in tests and (not trim_test in pre_tests or len(
          pre_tests[trim_test]) < len(trim_tests)):
        pre_tests[trim_test] = trim_tests
    else:
      other_tests.append(test)
  return pre_tests, other_tests


class _ApkDelegate:
  def __init__(self, test_instance, env):
    self._activity = test_instance.activity
    self._additional_apks = test_instance.additional_apks
    self._apk_helper = test_instance.apk_helper
    self._test_apk_incremental_install_json = (
        test_instance.test_apk_incremental_install_json)
    self._package = test_instance.package
    self._runner = test_instance.runner
    self._permissions = test_instance.permissions
    self._suite = test_instance.suite
    self._component = '%s/%s' % (self._package, self._runner)
    self._extras = test_instance.extras
    self._wait_for_java_debugger = test_instance.wait_for_java_debugger
    self._env = env
    self._coverage_dir = test_instance.coverage_dir
    self._coverage_index = 0
    self._use_existing_test_data = test_instance.use_existing_test_data

  def GetTestDataRoot(self, device):
    # pylint: disable=no-self-use
    return posixpath.join(device.GetExternalStoragePath(),
                          'chromium_tests_root')

  def Install(self, device):
    if self._use_existing_test_data:
      return

    for additional_apk in self._additional_apks:
      device.Install(additional_apk, allow_downgrade=True, reinstall=True)

    if self._test_apk_incremental_install_json:
      installer.Install(device, self._test_apk_incremental_install_json,
                        apk=self._apk_helper, permissions=self._permissions)
    else:
      device.Install(
          self._apk_helper,
          allow_downgrade=True,
          reinstall=True,
          permissions=self._permissions)

  def ResultsDirectory(self, device):  # pylint: disable=no-self-use
    return device.GetExternalStoragePath()

  def Run(self, test, device, flags=None, **kwargs):
    extras = dict(self._extras)
    device_api = device.build_version_sdk

    if self._coverage_dir and device_api >= version_codes.LOLLIPOP:
      # TODO(b/293175593): Use device.ResolveSpecialPath for multi-user
      device_coverage_dir = (
          code_coverage_utils.GetDeviceClangCoverageDir(device))
      extras[_EXTRA_COVERAGE_DEVICE_FILE] = _GetLLVMProfilePath(
          device_coverage_dir, self._suite, self._coverage_index)
      self._coverage_index += 1

    if ('timeout' in kwargs
        and gtest_test_instance.EXTRA_SHARD_NANO_TIMEOUT not in extras):
      # Make sure the instrumentation doesn't kill the test before the
      # scripts do. The provided timeout value is in seconds, but the
      # instrumentation deals with nanoseconds because that's how Android
      # handles time.
      extras[gtest_test_instance.EXTRA_SHARD_NANO_TIMEOUT] = int(
          kwargs['timeout'] * _SECONDS_TO_NANOS)

    command_line_file = _NullContextManager()
    if flags:
      if len(flags) > _MAX_INLINE_FLAGS_LENGTH:
        command_line_file = device_temp_file.DeviceTempFile(device.adb)
        device.WriteFile(command_line_file.name, '_ %s' % flags)
        extras[_EXTRA_COMMAND_LINE_FILE] = command_line_file.name
      else:
        extras[_EXTRA_COMMAND_LINE_FLAGS] = flags

    test_list_file = _NullContextManager()
    if test:
      if len(test) > 1:
        test_list_file = device_temp_file.DeviceTempFile(device.adb)
        device.WriteFile(test_list_file.name, '\n'.join(test))
        extras[_EXTRA_TEST_LIST] = test_list_file.name
      else:
        extras[_EXTRA_TEST] = test[0]

    # We need to use GetAppWritablePath here instead of GetExternalStoragePath
    # since we will not have yet applied legacy storage permission workarounds
    # on R+.
    stdout_file = device_temp_file.DeviceTempFile(
        device.adb,
        dir=device.GetAppWritablePath(),
        suffix='.gtest_out',
        device_utils=device)
    extras[_EXTRA_STDOUT_FILE] = stdout_file.name

    if self._wait_for_java_debugger:
      cmd = ['am', 'set-debug-app', '-w', self._package]
      device.RunShellCommand(cmd, check_return=True)
      logging.warning('*' * 80)
      logging.warning('Waiting for debugger to attach to process: %s',
                      self._package)
      logging.warning('*' * 80)

    with command_line_file, test_list_file, stdout_file:
      try:
        device.StartInstrumentation(
            self._component, extras=extras, raw=False, **kwargs)
      except device_errors.CommandFailedError:
        logging.exception('gtest shard failed.')
      except device_errors.CommandTimeoutError:
        logging.exception('gtest shard timed out.')
      except device_errors.DeviceUnreachableError:
        logging.exception('gtest shard device unreachable.')
      except Exception:
        device.ForceStop(self._package)
        raise
      finally:
        if self._coverage_dir and device_api >= version_codes.LOLLIPOP:
          if not os.path.isdir(self._coverage_dir):
            os.makedirs(self._coverage_dir)
          code_coverage_utils.PullAndMaybeMergeClangCoverageFiles(
              device, device_coverage_dir, self._coverage_dir,
              str(self._coverage_index))

      stdout_file_path = stdout_file.name
      if self._env.force_main_user:
        stdout_file_path = device.ResolveSpecialPath(stdout_file_path)
      stdout_file_content = device.ReadFile(stdout_file_path,
                                            as_root=self._env.force_main_user)
      return stdout_file_content.splitlines()

  def PullAppFiles(self, device, files, directory):
    device_dir = device.GetApplicationDataDirectory(self._package)
    host_dir = os.path.join(directory, str(device))
    for f in files:
      device_file = posixpath.join(device_dir, f)
      host_file = os.path.join(host_dir, *f.split(posixpath.sep))
      for host_file in _GenerateSequentialFileNames(host_file):
        if not os.path.exists(host_file):
          break
      device.PullFile(device_file, host_file)

  def Clear(self, device):
    device.ClearApplicationState(self._package, permissions=self._permissions)


class _ExeDelegate:

  def __init__(self, tr, test_instance, env):
    self._host_dist_dir = test_instance.exe_dist_dir
    self._exe_file_name = os.path.basename(
        test_instance.exe_dist_dir)[:-len('__dist')]
    self._device_dist_dir = posixpath.join(
        constants.TEST_EXECUTABLE_DIR,
        os.path.basename(test_instance.exe_dist_dir))
    self._test_run = tr
    self._env = env
    self._suite = test_instance.suite
    self._coverage_dir = test_instance.coverage_dir
    self._coverage_index = 0

  def GetTestDataRoot(self, device):
    # pylint: disable=no-self-use
    # pylint: disable=unused-argument
    return posixpath.join(constants.TEST_EXECUTABLE_DIR, 'chromium_tests_root')

  def Install(self, device):
    # TODO(jbudorick): Look into merging this with normal data deps pushing if
    # executables become supported on nonlocal environments.
    device.PushChangedFiles([(self._host_dist_dir, self._device_dist_dir)],
                            delete_device_stale=True,
                            as_root=self._env.force_main_user)

  def ResultsDirectory(self, device):
    # pylint: disable=no-self-use
    # pylint: disable=unused-argument
    return constants.TEST_EXECUTABLE_DIR

  def Run(self, test, device, flags=None, **kwargs):
    cmd = [posixpath.join(self._device_dist_dir, self._exe_file_name)]

    if test:
      cmd.append('--gtest_filter=%s' % ':'.join(test))
    if flags:
      # TODO(agrieve): This won't work if multiple flags are passed.
      cmd.append(flags)
    cwd = constants.TEST_EXECUTABLE_DIR

    env = {
        'LD_LIBRARY_PATH': self._device_dist_dir,
        'UBSAN_OPTIONS': constants.UBSAN_OPTIONS,
    }

    if self._coverage_dir:
      device_coverage_dir = (
          code_coverage_utils.GetDeviceClangCoverageDir(device))
      env['LLVM_PROFILE_FILE'] = _GetLLVMProfilePath(
          device_coverage_dir, self._suite, self._coverage_index)
      self._coverage_index += 1


    try:
      gcov_strip_depth = os.environ['NATIVE_COVERAGE_DEPTH_STRIP']
      external = device.GetExternalStoragePath()
      env['GCOV_PREFIX'] = '%s/gcov' % external
      env['GCOV_PREFIX_STRIP'] = gcov_strip_depth
    except (device_errors.CommandFailedError, KeyError):
      pass

    # Executable tests return a nonzero exit code on test failure, which is
    # fine from the test runner's perspective; thus check_return=False.
    output = device.RunShellCommand(
        cmd, cwd=cwd, env=env, check_return=False, large_output=True, **kwargs)

    if self._coverage_dir:
      # TODO(b/293175593): Use device.ResolveSpecialPath for multi-user
      code_coverage_utils.PullAndMaybeMergeClangCoverageFiles(
          device, device_coverage_dir, self._coverage_dir,
          str(self._coverage_index))

    return output

  def PullAppFiles(self, device, files, directory):
    pass

  def Clear(self, device):
    device.KillAll(self._exe_file_name,
                   blocking=True,
                   timeout=30 * _GetDeviceTimeoutMultiplier(),
                   quiet=True)


class LocalDeviceGtestRun(local_device_test_run.LocalDeviceTestRun):

  def __init__(self, env, test_instance):
    assert isinstance(env, local_device_environment.LocalDeviceEnvironment)
    assert isinstance(test_instance, gtest_test_instance.GtestTestInstance)
    super().__init__(env, test_instance)

    if self._test_instance.apk_helper:
      self._installed_packages = [
          self._test_instance.apk_helper.GetPackageName()
      ]

    if self._test_instance.apk:
      self._delegate = _ApkDelegate(self._test_instance, self._env)
    elif self._test_instance.exe_dist_dir:
      self._delegate = _ExeDelegate(self, self._test_instance, self._env)
    if self._test_instance.isolated_script_test_perf_output:
      self._test_perf_output_filenames = _GenerateSequentialFileNames(
          self._test_instance.isolated_script_test_perf_output)
    else:
      self._test_perf_output_filenames = itertools.repeat(None)
    self._crashes = set()
    self._servers = collections.defaultdict(list)

  #override
  def TestPackage(self):
    return self._test_instance.suite

  #override
  def SetUp(self):
    @local_device_environment.handle_shard_failures_with(
        on_failure=self._env.DenylistDevice)
    @trace_event.traced
    def individual_device_set_up(device, host_device_tuples):
      def install_apk(dev):
        # Install test APK.
        self._delegate.Install(dev)

      def push_test_data(dev):
        if self._test_instance.use_existing_test_data:
          return
        # Push data dependencies.
        device_root = self._delegate.GetTestDataRoot(dev)
        if self._env.force_main_user:
          device_root = dev.ResolveSpecialPath(device_root)
        resolved_host_device_tuples = device_dependencies.SubstituteDeviceRoot(
            host_device_tuples, device_root)
        dev.PlaceNomediaFile(device_root)
        dev.PushChangedFiles(
            resolved_host_device_tuples,
            delete_device_stale=True,
            as_root=self._env.force_main_user,
            # Some gtest suites, e.g. unit_tests, have data dependencies that
            # can take longer than the default timeout to push. See
            # crbug.com/791632 for context.
            timeout=600 * math.ceil(_GetDeviceTimeoutMultiplier() / 10))
        if not resolved_host_device_tuples:
          dev.RemovePath(device_root,
                         force=True,
                         recursive=True,
                         rename=True,
                         as_root=self._env.force_main_user)
          dev.RunShellCommand(['mkdir', '-p', device_root],
                              check_return=True,
                              as_root=self._env.force_main_user)

      def start_servers(dev):
        if self._env.disable_test_server:
          logging.warning('Not starting test server. Some tests may fail.')
          return

        try:
          # See https://crbug.com/1030827.
          # This is a hack that may break in the future. We're relying on the
          # fact that adb doesn't use ipv6 for it's server, and so doesn't
          # listen on ipv6, but ssh remote forwarding does. 5037 is the port
          # number adb uses for its server.
          if b"[::1]:5037" in subprocess.check_output(
              "ss -o state listening 'sport = 5037'", shell=True):
            logging.error(
                'Test Server cannot be started with a remote-forwarded adb '
                'server. Continuing anyways, but some tests may fail.')
            return
        except subprocess.CalledProcessError:
          pass

        self._servers[str(dev)] = []
        if self.TestPackage() in _SUITE_REQUIRES_TEST_SERVER_SPAWNER:
          self._servers[str(dev)].append(
              local_test_server_spawner.LocalTestServerSpawner(
                  ports.AllocateTestServerPort(), dev))

        for s in self._servers[str(dev)]:
          s.SetUp()

      def bind_crash_handler(step, dev):
        return lambda: crash_handler.RetryOnSystemCrash(step, dev)

      steps = [
          bind_crash_handler(s, device)
          for s in (install_apk, push_test_data, start_servers)
      ]
      if self._env.concurrent_adb:
        reraiser_thread.RunAsync(steps)
      else:
        for step in steps:
          step()

    self._env.parallel_devices.pMap(
        individual_device_set_up,
        self._test_instance.GetDataDependencies())

  #override
  def _ShouldShardTestsForDevices(self):
    """Shard tests across several devices.

    Returns:
      True if tests should be sharded across several devices,
      False otherwise.
    """
    return True

  #override
  def _CreateShardsForDevices(self, tests):
    """Create shards of tests to run on devices.

    Args:
      tests: List containing tests or test batches.

    Returns:
      List of test batches.
    """
    # _crashes are tests that might crash and make the tests in the same shard
    # following the crashed testcase not run.
    # Thus we need to create separate shards for each crashed testcase,
    # so that other tests can be run.
    device_count = len(self._env.devices)
    shards = []

    # Add shards with only one suspect testcase.
    shards += [[crash] for crash in self._crashes if crash in tests]

    # Delete suspect testcase from tests.
    tests = [test for test in tests if not test in self._crashes]

    max_shard_size = self._test_instance.test_launcher_batch_limit

    shards.extend(self._PartitionTests(tests, device_count, max_shard_size))
    return shards

  #override
  def _GetTests(self):
    if self._test_instance.extract_test_list_from_filter:
      # When the exact list of tests to run is given via command-line (e.g. when
      # locally iterating on a specific test), skip querying the device (which
      # takes ~3 seconds).
      tests = _ExtractTestsFromFilters(self._test_instance.gtest_filters)
      if tests:
        return tests

    # Even when there's only one device, it still makes sense to retrieve the
    # test list so that tests can be split up and run in batches rather than all
    # at once (since test output is not streamed).
    @local_device_environment.handle_shard_failures_with(
        on_failure=self._env.DenylistDevice)
    def list_tests(dev):
      timeout = 30 * _GetDeviceTimeoutMultiplier()
      retries = 1
      if self._test_instance.wait_for_java_debugger:
        timeout = None

      flags = [
          f for f in self._test_instance.flags if f not in [
              '--wait-for-debugger', '--wait-for-java-debugger',
              '--gtest_also_run_disabled_tests'
          ]
      ]
      flags.append('--gtest_list_tests')

      # TODO(crbug.com/40522854): Remove retries when no longer necessary.
      for i in range(0, retries + 1):
        logging.info('flags:')
        for f in flags:
          logging.info('  %s', f)

        with self._ArchiveLogcat(dev, 'list_tests'):
          raw_test_list = crash_handler.RetryOnSystemCrash(
              lambda d: self._delegate.Run(
                  None, d, flags=' '.join(flags), timeout=timeout),
              device=dev)

        tests = gtest_test_instance.ParseGTestListTests(raw_test_list)
        if not tests:
          logging.info('No tests found. Output:')
          for l in raw_test_list:
            logging.info('  %s', l)
          if i < retries:
            logging.info('Retrying...')
        else:
          break
      return tests

    # Query all devices in case one fails.
    test_lists = self._env.parallel_devices.pMap(list_tests).pGet(None)

    # If all devices failed to list tests, raise an exception.
    # Check that tl is not None and is not empty.
    if all(not tl for tl in test_lists):
      raise device_errors.CommandFailedError(
          'Failed to list tests on any device')
    tests = list(sorted(set().union(*[set(tl) for tl in test_lists if tl])))
    tests = self._test_instance.FilterTests(tests)
    tests = self._ApplyExternalSharding(
        tests, self._test_instance.external_shard_index,
        self._test_instance.total_external_shards)
    return tests

  #override
  def _AppendPreTestsForRetry(self, failed_tests, tests):
    if not self._test_instance.run_pre_tests:
      return failed_tests

    pre_tests, _ = _GroupPreTests(tests)
    trim_failed_tests = set()
    for failed_test in failed_tests:
      failed_test_name_start = max(failed_test.find('.') + 1, 0)
      failed_test_name = failed_test[failed_test_name_start:]

      if failed_test_name_start > 0 and failed_test_name.startswith(
          _GTEST_PRETEST_PREFIX):
        failed_test_suite = failed_test[:failed_test_name_start - 1]
        while failed_test_name.startswith(_GTEST_PRETEST_PREFIX):
          failed_test_name = failed_test_name[len(_GTEST_PRETEST_PREFIX):]
        failed_test = '%s.%s' % (failed_test_suite, failed_test_name)
      trim_failed_tests.add(failed_test)

    all_tests = []
    for trim_failed_test in trim_failed_tests:
      if trim_failed_test in tests:
        if trim_failed_test in pre_tests:
          all_tests.extend(pre_tests[trim_failed_test])
        else:
          all_tests.append(trim_failed_test)
    return all_tests

  #override
  def _GroupTests(self, tests):
    pre_tests, other_tests = _GroupPreTests(tests)

    all_tests = []
    for other_test in other_tests:
      if not other_test in pre_tests:
        all_tests.append(other_test)

    # TODO(crbug.com/40200835): Add logic to support grouping tests.
    # Once grouping logic is added, switch to 'append' from 'extend'.
    for _, test_list in pre_tests.items():
      all_tests.extend(test_list)

    return all_tests

  #override
  def _GroupTestsAfterSharding(self, tests):
    return self._GroupTests(tests)

  def _UploadTestArtifacts(self, device, test_artifacts_device_dir):
    # TODO(jbudorick): Reconcile this with the output manager once
    # https://codereview.chromium.org/2933993002/ lands.
    if self._env.force_main_user:
      test_artifacts_device_dir = device.ResolveSpecialPath(
          test_artifacts_device_dir)

    with tempfile_ext.NamedTemporaryDirectory() as test_artifacts_host_dir:
      device.PullFile(test_artifacts_device_dir,
                      test_artifacts_host_dir,
                      as_root=self._env.force_main_user)
      with tempfile_ext.NamedTemporaryDirectory() as temp_zip_dir:
        zip_base_name = os.path.join(temp_zip_dir, 'test_artifacts')
        test_artifacts_zip = shutil.make_archive(zip_base_name, 'zip',
                                                 test_artifacts_host_dir)
        link = google_storage_helper.upload(
            google_storage_helper.unique_name('test_artifacts', device=device),
            test_artifacts_zip,
            bucket='%s/test_artifacts' %
            (self._test_instance.gs_test_artifacts_bucket))
        logging.info('Uploading test artifacts to %s.', link)
        return link

  def _PullRenderTestOutput(self, device, render_test_output_device_dir):
    # We pull the render tests into a temp directory then copy them over
    # individually. Otherwise we end up with a temporary directory name
    # in the host output directory.
    if self._env.force_main_user:
      render_test_output_device_dir = device.ResolveSpecialPath(
          render_test_output_device_dir)

    with tempfile_ext.NamedTemporaryDirectory() as tmp_host_dir:
      try:
        device.PullFile(render_test_output_device_dir,
                        tmp_host_dir,
                        as_root=self._env.force_main_user)
      except device_errors.CommandFailedError:
        logging.exception('Failed to pull render test output dir %s',
                          render_test_output_device_dir)
      temp_host_dir = os.path.join(
          tmp_host_dir, os.path.basename(render_test_output_device_dir))
      for output_file in os.listdir(temp_host_dir):
        src_path = os.path.join(temp_host_dir, output_file)
        dst_path = os.path.join(self._test_instance.render_test_output_dir,
                                output_file)
        shutil.move(src_path, dst_path)

  @contextlib.contextmanager
  def _ArchiveLogcat(self, device, test):
    if isinstance(test, str):
      desc = test
    else:
      desc = hash(tuple(test))

    stream_name = 'logcat_%s_shard%s_%s_%s' % (
        desc, self._test_instance.external_shard_index,
        time.strftime('%Y%m%dT%H%M%S-UTC', time.gmtime()), device.serial)

    logcat_file = None
    logmon = None
    try:
      with self._env.output_manager.ArchivedTempfile(stream_name,
                                                     'logcat') as logcat_file:
        symbolizer = stack_symbolizer.PassThroughSymbolizerPool(
            device.product_cpu_abi)
        with symbolizer:
          with logcat_monitor.LogcatMonitor(
              device.adb,
              filter_specs=local_device_environment.LOGCAT_FILTERS,
              output_file=logcat_file.name,
              transform_func=symbolizer.TransformLines,
              check_error=False) as logmon:
            with contextlib_ext.Optional(trace_event.trace(str(test)),
                                         self._env.trace_output):
              yield logcat_file
    finally:
      if logmon:
        logmon.Close()
      if logcat_file and logcat_file.Link():
        logging.critical('Logcat saved to %s', logcat_file.Link())

  #override
  def _GetUniqueTestName(self, test):
    return gtest_test_instance.TestNameWithoutDisabledPrefix(test)

  #override
  def _RunTest(self, device, test):
    # Run the test.
    timeout = self._test_instance.shard_timeout * _GetDeviceTimeoutMultiplier()
    if self._test_instance.wait_for_java_debugger:
      timeout = None
    if self._test_instance.store_tombstones:
      tombstones.ClearAllTombstones(device)
    test_perf_output_filename = next(self._test_perf_output_filenames)

    if self._test_instance.isolated_script_test_output:
      suffix = '.json'
    else:
      suffix = '.xml'

    with device_temp_file.DeviceTempFile(
        adb=device.adb,
        dir=self._delegate.ResultsDirectory(device),
        suffix=suffix,
        device_utils=device) as device_tmp_results_file:
      with contextlib_ext.Optional(
          device_temp_file.NamedDeviceTemporaryDirectory(
              adb=device.adb,
              dir=device.GetExternalStoragePath(),
              device_utils=device),
          self._test_instance.gs_test_artifacts_bucket) as test_artifacts_dir:
        with contextlib_ext.Optional(
            device_temp_file.DeviceTempFile(
                adb=device.adb,
                dir=self._delegate.ResultsDirectory(device),
                device_utils=device),
            test_perf_output_filename) as isolated_script_test_perf_output:
          with contextlib_ext.Optional(
              device_temp_file.NamedDeviceTemporaryDirectory(
                  adb=device.adb,
                  dir=device.GetExternalStoragePath(),
                  device_utils=device),
              self._test_instance.render_test_output_dir
          ) as render_test_output_dir:

            flags = list(self._test_instance.flags)
            if self._test_instance.enable_xml_result_parsing:
              flags.append('--gtest_output=xml:%s' %
                           device_tmp_results_file.name)

            if self._test_instance.gs_test_artifacts_bucket:
              flags.append('--test_artifacts_dir=%s' % test_artifacts_dir.name)

            if self._test_instance.isolated_script_test_output:
              flags.append('--isolated-script-test-output=%s' %
                           device_tmp_results_file.name)

            if test_perf_output_filename:
              flags.append('--isolated_script_test_perf_output=%s' %
                           isolated_script_test_perf_output.name)

            if self._test_instance.render_test_output_dir:
              flags.append('--render-test-output-dir=%s' %
                           render_test_output_dir.name)

            logging.info('flags:')
            for f in flags:
              logging.info('  %s', f)

            with self._ArchiveLogcat(device, test) as logcat_file:
              output = self._delegate.Run(test,
                                          device,
                                          flags=' '.join(flags),
                                          timeout=timeout,
                                          retries=0)

            if self._test_instance.enable_xml_result_parsing:
              file_path = device_tmp_results_file.name
              if self._env.force_main_user:
                file_path = device.ResolveSpecialPath(file_path)
              try:
                gtest_xml = device.ReadFile(file_path,
                                            as_root=self._env.force_main_user)
              except device_errors.CommandFailedError:
                logging.exception('Failed to pull gtest results XML file %s',
                                  file_path)
                gtest_xml = None

            if self._test_instance.isolated_script_test_output:
              file_path = device_tmp_results_file.name
              if self._env.force_main_user:
                file_path = device.ResolveSpecialPath(file_path)
              try:
                gtest_json = device.ReadFile(file_path,
                                             as_root=self._env.force_main_user)
              except device_errors.CommandFailedError:
                logging.exception('Failed to pull gtest results JSON file %s',
                                  file_path)
                gtest_json = None

            if test_perf_output_filename:
              file_path = isolated_script_test_perf_output.name
              if self._env.force_main_user:
                file_path = device.ResolveSpecialPath(file_path)
              try:
                device.PullFile(file_path,
                                test_perf_output_filename,
                                as_root=self._env.force_main_user)
              except device_errors.CommandFailedError:
                logging.exception('Failed to pull chartjson results %s',
                                  file_path)

            test_artifacts_url = None
            if test_artifacts_dir:
              test_artifacts_url = self._UploadTestArtifacts(
                  device, test_artifacts_dir.name)

            if render_test_output_dir:
              self._PullRenderTestOutput(device, render_test_output_dir.name)

    for s in self._servers[str(device)]:
      s.Reset()
    if self._test_instance.app_files:
      self._delegate.PullAppFiles(device, self._test_instance.app_files,
                                  self._test_instance.app_file_dir)
    if not self._env.skip_clear_data:
      self._delegate.Clear(device)

    for l in output:
      logging.info(l)

    # Parse the output.
    # TODO(crbug.com/366267015): Transition test scripts away from parsing
    # stdout.
    if self._test_instance.enable_xml_result_parsing:
      results = gtest_test_instance.ParseGTestXML(gtest_xml)
    elif self._test_instance.isolated_script_test_output:
      results = gtest_test_instance.ParseGTestJSON(gtest_json)
    else:
      results = gtest_test_instance.ParseGTestOutput(
          output, self._test_instance.symbolizer, device.product_cpu_abi)

    tombstones_url = None
    for r in results:
      if logcat_file:
        r.SetLink('logcat', logcat_file.Link())

      if self._test_instance.gs_test_artifacts_bucket:
        r.SetLink('test_artifacts', test_artifacts_url)

      if r.GetType() == base_test_result.ResultType.CRASH:
        self._crashes.add(r.GetName())
        if self._test_instance.store_tombstones:
          if not tombstones_url:
            resolved_tombstones = tombstones.ResolveTombstones(
                device,
                resolve_all_tombstones=True,
                include_stack_symbols=False,
                wipe_tombstones=True)
            stream_name = 'tombstones_%s_%s' % (
                time.strftime('%Y%m%dT%H%M%S', time.localtime()),
                device.serial)
            tombstones_url = logdog_helper.text(
                stream_name, '\n'.join(resolved_tombstones))
          r.SetLink('tombstones', tombstones_url)

    tests_stripped_disabled_prefix = set()
    for t in test:
      tests_stripped_disabled_prefix.add(
          gtest_test_instance.TestNameWithoutDisabledPrefix(t))
    not_run_tests = tests_stripped_disabled_prefix.difference(
        set(r.GetName() for r in results))

    if self._test_instance.extract_test_list_from_filter:
      # A test string might end with a * in this mode, and so may not match any
      # r.GetName() for the set difference. It's possible a filter like foo.*
      # can match two tests, ie foo.baz and foo.foo.
      # When running it's possible Foo.baz is ran, foo.foo is not, but the test
      # list foo.* will not be reran as at least one result matched it.
      not_run_tests = {
          t
          for t in not_run_tests
          if not any(fnmatch.fnmatch(r.GetName(), t) for r in results)
      }

    return results, list(not_run_tests) if results else None

  #override
  def TearDown(self):
    # By default, teardown will invoke ADB. When receiving SIGTERM due to a
    # timeout, there's a high probability that ADB is non-responsive. In these
    # cases, sending an ADB command will potentially take a long time to time
    # out. Before this happens, the process will be hard-killed for not
    # responding to SIGTERM fast enough.
    if self._received_sigterm:
      return

    @local_device_environment.handle_shard_failures
    @trace_event.traced
    def individual_device_tear_down(dev):
      for s in self._servers.get(str(dev), []):
        s.TearDown()

    self._env.parallel_devices.pMap(individual_device_tear_down)
