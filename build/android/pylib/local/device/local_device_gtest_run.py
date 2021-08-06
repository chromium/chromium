# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


import contextlib
import collections
import itertools
import logging
import math
import os
import posixpath
import subprocess
import shutil
import time

from six.moves import range  # pylint: disable=redefined-builtin
from devil import base_error
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

_SECONDS_TO_NANOS = int(1e9)

# Tests that use SpawnedTestServer must run the LocalTestServerSpawner on the
# host machine.
# TODO(jbudorick): Move this up to the test instance if the net test server is
# handled outside of the APK for the remote_device environment.
_SUITE_REQUIRES_TEST_SERVER_SPAWNER = [
  'components_browsertests', 'content_unittests', 'content_browsertests',
  'net_unittests', 'services_unittests', 'unit_tests'
]

# These are use for code coverage.
_LLVM_PROFDATA_PATH = os.path.join(constants.DIR_SOURCE_ROOT, 'third_party',
                                   'llvm-build', 'Release+Asserts', 'bin',
                                   'llvm-profdata')
# Name of the file extension for profraw data files.
_PROFRAW_FILE_EXTENSION = 'profraw'
# Name of the file where profraw data files are merged.
_MERGE_PROFDATA_FILE_NAME = 'coverage_merged.' + _PROFRAW_FILE_EXTENSION

# No-op context manager. If we used Python 3, we could change this to
# contextlib.ExitStack()
class _NullContextManager(object):
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


def _ExtractTestsFromFilter(gtest_filter):
  """Returns the list of tests specified by the given filter.

  Returns:
    None if the device should be queried for the test list instead.
  """
  # Empty means all tests, - means exclude filter.
  if not gtest_filter or '-' in gtest_filter:
    return None

  patterns = gtest_filter.split(':')
  # For a single pattern, allow it even if it has a wildcard so long as the
  # wildcard comes at the end and there is at least one . to prove the scope is
  # not too large.
  # This heuristic is not necessarily faster, but normally is.
  if len(patterns) == 1 and patterns[0].endswith('*'):
    no_suffix = patterns[0].rstrip('*')
    if '*' not in no_suffix and '.' in no_suffix:
      return patterns

  if '*' in gtest_filter:
    return None
  return patterns


def _GetDeviceTimeoutMultiplier():
  # Emulated devices typically run 20-150x slower than real-time.
  # Give a way to control this through the DEVICE_TIMEOUT_MULTIPLIER
  # environment variable.
  multiplier = os.getenv("DEVICE_TIMEOUT_MULTIPLIER")
  if multiplier:
    return int(multiplier)
  return 1


def _MergeCoverageFiles(coverage_dir, profdata_dir):
  """Merge coverage data files.

  Each instrumentation activity generates a separate profraw data file. This
  merges all profraw files in profdata_dir into a single file in
  coverage_dir. This happens after each test, rather than waiting until after
  all tests are ran to reduce the memory footprint used by all the profraw
  files.

  Args:
    coverage_dir: The path to the coverage directory.
    profdata_dir: The directory where the profraw data file(s) are located.

  Return:
    None
  """
  # profdata_dir may not exist if pulling coverage files failed.
  if not os.path.exists(profdata_dir):
    logging.debug('Profraw directory does not exist.')
    return

  merge_file = os.path.join(coverage_dir, _MERGE_PROFDATA_FILE_NAME)
  profraw_files = [
      os.path.join(profdata_dir, f) for f in os.listdir(profdata_dir)
      if f.endswith(_PROFRAW_FILE_EXTENSION)
  ]

  try:
    logging.debug('Merging target profraw files into merged profraw file.')
    subprocess_cmd = [
        _LLVM_PROFDATA_PATH,
        'merge',
        '-o',
        merge_file,
        '-sparse=true',
    ]
    # Grow the merge file by merging it with itself and the new files.
    if os.path.exists(merge_file):
      subprocess_cmd.append(merge_file)
    subprocess_cmd.extend(profraw_files)
    output = subprocess.check_output(subprocess_cmd)
    logging.debug('Merge output: %s', output)
  except subprocess.CalledProcessError:
    # Don't raise error as that will kill the test run. When code coverage
    # generates a report, that will raise the error in the report generation.
    logging.error(
        'Failed to merge target profdata files to create merged profraw file.')

  # Free up memory space on bot as all data is in the merge file.
  for f in profraw_files:
    os.remove(f)


def _PullCoverageFiles(device, device_coverage_dir, output_dir):
  """Pulls coverage files on device to host directory.

  Args:
    device: The working device.
    device_coverage_dir: The directory to store coverage data on device.
    output_dir: The output directory on host.
  """
  try:
    if not os.path.exists(output_dir):
      os.makedirs(output_dir)
    device.PullFile(device_coverage_dir, output_dir)
    if not os.listdir(os.path.join(output_dir, 'profraw')):
      logging.warning('No coverage data was generated for this run')
  except (OSError, base_error.BaseError) as e:
    logging.warning('Failed to handle coverage data after tests: %s', e)
  finally:
    device.RemovePath(device_coverage_dir, force=True, recursive=True)


def _GetDeviceCoverageDir(device):
  """Gets the directory to generate coverage data on device.

  Args:
    device: The working device.

  Returns:
    The directory path on the device.
  """
  return posixpath.join(device.GetExternalStoragePath(), 'chrome', 'test',
                        'coverage', 'profraw')


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
  return posixpath.join(device_coverage_dir,
                        '_'.join([suite,
                                  str(coverage_index), '%2m.profraw']))


class _ApkDelegate(object):
  def __init__(self, test_instance, tool):
    self._activity = test_instance.activity
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
    self._tool = tool
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
    if self._test_apk_incremental_install_json:
      installer.Install(device, self._test_apk_incremental_install_json,
                        apk=self._apk_helper, permissions=self._permissions)
    else:
      device.Install(
          self._apk_helper,
          allow_downgrade=True,
          reinstall=True,
          permissions=self._permissions)

  def ResultsDirectory(self, device):
    return device.GetApplicationDataDirectory(self._package)

  def Run(self, test, device, flags=None, **kwargs):
    extras = dict(self._extras)
    device_api = device.build_version_sdk

    if self._coverage_dir and device_api >= version_codes.LOLLIPOP:
      device_coverage_dir = _GetDeviceCoverageDir(device)
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

    # pylint: disable=redefined-variable-type
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
    # pylint: enable=redefined-variable-type

    # We need to use GetAppWritablePath here instead of GetExternalStoragePath
    # since we will not have yet applied legacy storage permission workarounds
    # on R+.
    stdout_file = device_temp_file.DeviceTempFile(
        device.adb, dir=device.GetAppWritablePath(), suffix='.gtest_out')
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
        # TODO(crbug.com/1179004) Use _MergeCoverageFiles when llvm-profdata
        # not found is fixed.
          _PullCoverageFiles(
              device, device_coverage_dir,
              os.path.join(self._coverage_dir, str(self._coverage_index)))

      return device.ReadFile(stdout_file.name).splitlines()

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


class _ExeDelegate(object):

  def __init__(self, tr, test_instance, tool):
    self._host_dist_dir = test_instance.exe_dist_dir
    self._exe_file_name = os.path.basename(
        test_instance.exe_dist_dir)[:-len('__dist')]
    self._device_dist_dir = posixpath.join(
        constants.TEST_EXECUTABLE_DIR,
        os.path.basename(test_instance.exe_dist_dir))
    self._test_run = tr
    self._tool = tool
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
                            delete_device_stale=True)

  def ResultsDirectory(self, device):
    # pylint: disable=no-self-use
    # pylint: disable=unused-argument
    return constants.TEST_EXECUTABLE_DIR

  def Run(self, test, device, flags=None, **kwargs):
    tool = self._test_run.GetTool(device).GetTestWrapper()
    if tool:
      cmd = [tool]
    else:
      cmd = []
    cmd.append(posixpath.join(self._device_dist_dir, self._exe_file_name))

    if test:
      cmd.append('--gtest_filter=%s' % ':'.join(test))
    if flags:
      # TODO(agrieve): This won't work if multiple flags are passed.
      cmd.append(flags)
    cwd = constants.TEST_EXECUTABLE_DIR

    env = {
      'LD_LIBRARY_PATH': self._device_dist_dir
    }

    if self._coverage_dir:
      device_coverage_dir = _GetDeviceCoverageDir(device)
      env['LLVM_PROFILE_FILE'] = _GetLLVMProfilePath(
          device_coverage_dir, self._suite, self._coverage_index)
      self._coverage_index += 1

    if self._tool != 'asan':
      env['UBSAN_OPTIONS'] = constants.UBSAN_OPTIONS

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
      _PullCoverageFiles(
          device, device_coverage_dir,
          os.path.join(self._coverage_dir, str(self._coverage_index)))

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
    super(LocalDeviceGtestRun, self).__init__(env, test_instance)

    if self._test_instance.apk_helper:
      self._installed_packages = [
          self._test_instance.apk_helper.GetPackageName()
      ]

    # pylint: disable=redefined-variable-type
    if self._test_instance.apk:
      self._delegate = _ApkDelegate(self._test_instance, env.tool)
    elif self._test_instance.exe_dist_dir:
      self._delegate = _ExeDelegate(self, self._test_instance, self._env.tool)
    if self._test_instance.isolated_script_test_perf_output:
      self._test_perf_output_filenames = _GenerateSequentialFileNames(
          self._test_instance.isolated_script_test_perf_output)
    else:
      self._test_perf_output_filenames = itertools.repeat(None)
    # pylint: enable=redefined-variable-type
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
        host_device_tuples_substituted = [
            (h, local_device_test_run.SubstituteDeviceRoot(d, device_root))
            for h, d in host_device_tuples]
        local_device_environment.place_nomedia_on_device(dev, device_root)
        dev.PushChangedFiles(
            host_device_tuples_substituted,
            delete_device_stale=True,
            # Some gtest suites, e.g. unit_tests, have data dependencies that
            # can take longer than the default timeout to push. See
            # crbug.com/791632 for context.
            timeout=600 * math.ceil(_GetDeviceTimeoutMultiplier() / 10))
        if not host_device_tuples:
          dev.RemovePath(device_root, force=True, recursive=True, rename=True)
          dev.RunShellCommand(['mkdir', '-p', device_root], check_return=True)

      def init_tool_and_start_servers(dev):
        tool = self.GetTool(dev)
        tool.CopyFiles(dev)
        tool.SetupEnvironment()

        try:
          # See https://crbug.com/1030827.
          # This is a hack that may break in the future. We're relying on the
          # fact that adb doesn't use ipv6 for it's server, and so doesn't
          # listen on ipv6, but ssh remote forwarding does. 5037 is the port
          # number adb uses for its server.
          if "[::1]:5037" in subprocess.check_output(
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
                  ports.AllocateTestServerPort(), dev, tool))

        for s in self._servers[str(dev)]:
          s.SetUp()

      def bind_crash_handler(step, dev):
        return lambda: crash_handler.RetryOnSystemCrash(step, dev)

      # Explicitly enable root to ensure that tests run under deterministic
      # conditions. Without this explicit call, EnableRoot() is called from
      # push_test_data() when PushChangedFiles() determines that it should use
      # _PushChangedFilesZipped(), which is only most of the time.
      # Root is required (amongst maybe other reasons) to pull the results file
      # from the device, since it lives within the application's data directory
      # (via GetApplicationDataDirectory()).
      device.EnableRoot()

      steps = [
          bind_crash_handler(s, device)
          for s in (install_apk, push_test_data, init_tool_and_start_servers)]
      if self._env.concurrent_adb:
        reraiser_thread.RunAsync(steps)
      else:
        for step in steps:
          step()

    self._env.parallel_devices.pMap(
        individual_device_set_up,
        self._test_instance.GetDataDependencies())

  #override
  def _ShouldShard(self):
    return True

  #override
  def _CreateShards(self, tests):
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
      tests = _ExtractTestsFromFilter(self._test_instance.gtest_filter)
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
          f for f in self._test_instance.flags
          if f not in ['--wait-for-debugger', '--wait-for-java-debugger']
      ]
      flags.append('--gtest_list_tests')

      # TODO(crbug.com/726880): Remove retries when no longer necessary.
      for i in range(0, retries+1):
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

  def _UploadTestArtifacts(self, device, test_artifacts_dir):
    # TODO(jbudorick): Reconcile this with the output manager once
    # https://codereview.chromium.org/2933993002/ lands.
    if test_artifacts_dir:
      with tempfile_ext.NamedTemporaryDirectory() as test_artifacts_host_dir:
        device.PullFile(test_artifacts_dir.name, test_artifacts_host_dir)
        with tempfile_ext.NamedTemporaryDirectory() as temp_zip_dir:
          zip_base_name = os.path.join(temp_zip_dir, 'test_artifacts')
          test_artifacts_zip = shutil.make_archive(
              zip_base_name, 'zip', test_artifacts_host_dir)
          link = google_storage_helper.upload(
              google_storage_helper.unique_name(
                  'test_artifacts', device=device),
              test_artifacts_zip,
              bucket='%s/test_artifacts' % (
                  self._test_instance.gs_test_artifacts_bucket))
          logging.info('Uploading test artifacts to %s.', link)
          return link
    return None

  def _PullRenderTestOutput(self, device, render_test_output_device_dir):
    # We pull the render tests into a temp directory then copy them over
    # individually. Otherwise we end up with a temporary directory name
    # in the host output directory.
    with tempfile_ext.NamedTemporaryDirectory() as tmp_host_dir:
      try:
        device.PullFile(render_test_output_device_dir, tmp_host_dir)
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
        with logcat_monitor.LogcatMonitor(
            device.adb,
            filter_specs=local_device_environment.LOGCAT_FILTERS,
            output_file=logcat_file.name,
            check_error=False) as logmon:
          with contextlib_ext.Optional(trace_event.trace(str(test)),
                                       self._env.trace_output):
            yield logcat_file
    finally:
      if logmon:
        logmon.Close()
      if logcat_file and logcat_file.Link():
        logging.info('Logcat saved to %s', logcat_file.Link())

  #override
  def _RunTest(self, device, test):
    # Run the test.
    timeout = (self._test_instance.shard_timeout *
               self.GetTool(device).GetTimeoutScale() *
               _GetDeviceTimeoutMultiplier())
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
        suffix=suffix) as device_tmp_results_file:
      with contextlib_ext.Optional(
          device_temp_file.NamedDeviceTemporaryDirectory(
              adb=device.adb, dir='/sdcard/'),
          self._test_instance.gs_test_artifacts_bucket) as test_artifacts_dir:
        with (contextlib_ext.Optional(
            device_temp_file.DeviceTempFile(
                adb=device.adb, dir=self._delegate.ResultsDirectory(device)),
            test_perf_output_filename)) as isolated_script_test_perf_output:
          with contextlib_ext.Optional(
              device_temp_file.NamedDeviceTemporaryDirectory(adb=device.adb,
                                                             dir='/sdcard/'),
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
              try:
                gtest_xml = device.ReadFile(device_tmp_results_file.name)
              except device_errors.CommandFailedError:
                logging.exception('Failed to pull gtest results XML file %s',
                                  device_tmp_results_file.name)
                gtest_xml = None

            if self._test_instance.isolated_script_test_output:
              try:
                gtest_json = device.ReadFile(device_tmp_results_file.name)
              except device_errors.CommandFailedError:
                logging.exception('Failed to pull gtest results JSON file %s',
                                  device_tmp_results_file.name)
                gtest_json = None

            if test_perf_output_filename:
              try:
                device.PullFile(isolated_script_test_perf_output.name,
                                test_perf_output_filename)
              except device_errors.CommandFailedError:
                logging.exception('Failed to pull chartjson results %s',
                                  isolated_script_test_perf_output.name)

            test_artifacts_url = self._UploadTestArtifacts(
                device, test_artifacts_dir)

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
    # TODO(jbudorick): Transition test scripts away from parsing stdout.
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

      tool = self.GetTool(dev)
      tool.CleanUpEnvironment()

    self._env.parallel_devices.pMap(individual_device_tear_down)
