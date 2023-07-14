# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import itertools
import json
import logging
import multiprocessing
import os
import queue
import re
import subprocess
import sys
import tempfile
import threading
import time
import zipfile
from collections import defaultdict
from concurrent.futures import ThreadPoolExecutor

from six.moves import range  # pylint: disable=redefined-builtin
from devil.utils import cmd_helper
from py_utils import tempfile_ext
from pylib import constants
from pylib.base import base_test_result
from pylib.base import test_run
from pylib.constants import host_paths
from pylib.results import json_results

# Chosen after timing test runs of chrome_junit_tests with 7,16,32,
# and 64 workers in threadpool and different classes_per_job.
_MAX_TESTS_PER_JOB = 150

_FAILURE_TYPES = (
    base_test_result.ResultType.FAIL,
    base_test_result.ResultType.CRASH,
    base_test_result.ResultType.TIMEOUT,
)

# Test suites are broken up into batches or "jobs" of about 150 tests.
# Each job should take no longer than 30 seconds.
_JOB_TIMEOUT = 30

# RegExp to detect logcat lines, e.g., 'I/AssetManager: not found'.
_LOGCAT_RE = re.compile(r'(:?\d+\| )?[A-Z]/[\w\d_-]+:')

# Regex to detect start or failure of tests. Matches
# [ RUN      ] org.ui.ForeignSessionItemViewBinderUnitTest.test_phone[28]
# [ FAILED|CRASHED|TIMEOUT ] org.ui.ForeignBinderUnitTest.test_phone[28] (56 ms)
_TEST_START_RE = re.compile(r'.*\[\s+RUN\s+\]\s(.*)')
_TEST_FAILED_RE = re.compile(r'.*\[\s+(?:FAILED|CRASHED|TIMEOUT)\s+\]')

# Regex that matches a test name, and optionally matches the sdk version e.g.:
# org.chromium.default_browser_promo.PromoUtilsTest#testNoPromo[28]'
_TEST_SDK_VERSION = re.compile(r'(.*\.\w+)#\w+(?:\[(\d+)\])?')


class LocalMachineJunitTestRun(test_run.TestRun):
  # override
  def TestPackage(self):
    return self._test_instance.suite

  # override
  def SetUp(self):
    pass

  def _GetFilterArgs(self, shard_test_filter=None):
    ret = []
    if shard_test_filter:
      ret += ['-gtest-filter', ':'.join(shard_test_filter)]

    for test_filter in self._test_instance.test_filters:
      ret += ['-gtest-filter', test_filter]

    if self._test_instance.package_filter:
      ret += ['-package-filter', self._test_instance.package_filter]
    if self._test_instance.runner_filter:
      ret += ['-runner-filter', self._test_instance.runner_filter]

    return ret

  def _CreateJarArgsList(self, json_result_file_paths, grouped_tests, shards):
    # Creates a list of jar_args. The important thing is each jar_args list
    # has a different json_results file for writing test results to and that
    # each list of jar_args has its own test to run as specified in the
    # -gtest-filter.
    jar_args_list = [['-json-results-file', result_file]
                     for result_file in json_result_file_paths]
    for index, jar_arg in enumerate(jar_args_list):
      shard_test_filter = grouped_tests[index] if shards > 1 else None
      jar_arg += self._GetFilterArgs(shard_test_filter)

    return jar_args_list

  def _CreateJvmArgsList(self, for_listing=False):
    # Creates a list of jvm_args (robolectric, code coverage, etc...)
    jvm_args = [
        '-Drobolectric.dependency.dir=%s' %
        self._test_instance.robolectric_runtime_deps_dir,
        '-Ddir.source.root=%s' % constants.DIR_SOURCE_ROOT,
        # Use locally available sdk jars from 'robolectric.dependency.dir'
        '-Drobolectric.offline=true',
        '-Drobolectric.resourcesMode=binary',
        '-Drobolectric.logging=stdout',
        '-Djava.library.path=%s' % self._test_instance.native_libs_dir,
    ]
    if self._test_instance.debug_socket and not for_listing:
      jvm_args += [
          '-Dchromium.jdwp_active=true',
          ('-agentlib:jdwp=transport=dt_socket'
           ',server=y,suspend=y,address=%s' % self._test_instance.debug_socket)
      ]

    if self._test_instance.coverage_dir and not for_listing:
      if not os.path.exists(self._test_instance.coverage_dir):
        os.makedirs(self._test_instance.coverage_dir)
      elif not os.path.isdir(self._test_instance.coverage_dir):
        raise Exception('--coverage-dir takes a directory, not file path.')
      # Jacoco supports concurrent processes using the same output file:
      # https://github.com/jacoco/jacoco/blob/6cd3f0bd8e348f8fba7bffec5225407151f1cc91/org.jacoco.agent.rt/src/org/jacoco/agent/rt/internal/output/FileOutput.java#L67
      # So no need to vary the output based on shard number.
      jacoco_coverage_file = os.path.join(self._test_instance.coverage_dir,
                                          '%s.exec' % self._test_instance.suite)
      if self._test_instance.coverage_on_the_fly:
        jacoco_agent_path = os.path.join(host_paths.DIR_SOURCE_ROOT,
                                         'third_party', 'jacoco', 'lib',
                                         'jacocoagent.jar')

        # inclnolocationclasses is false to prevent no class def found error.
        jacoco_args = '-javaagent:{}=destfile={},inclnolocationclasses=false'
        jvm_args.append(
            jacoco_args.format(jacoco_agent_path, jacoco_coverage_file))
      else:
        jvm_args.append('-Djacoco-agent.destfile=%s' % jacoco_coverage_file)

    return jvm_args

  @property
  def _wrapper_path(self):
    return os.path.join(constants.GetOutDirectory(), 'bin', 'helper',
                        self._test_instance.suite)

  #override
  def GetTestsForListing(self):
    with tempfile_ext.NamedTemporaryDirectory() as temp_dir:
      cmd = [self._wrapper_path, '--list-tests'] + self._GetFilterArgs()
      jvm_args = self._CreateJvmArgsList(for_listing=True)
      if jvm_args:
        cmd += ['--jvm-args', '"%s"' % ' '.join(jvm_args)]
      AddPropertiesJar([cmd], temp_dir, self._test_instance.resource_apk)
      try:
        lines = subprocess.check_output(cmd, encoding='utf8').splitlines()
      except subprocess.CalledProcessError:
        # Will get an error later on from testrunner from having no tests.
        return []

    PREFIX = '#TEST# '
    prefix_len = len(PREFIX)
    # Filter log messages other than test names (Robolectric logs to stdout).
    return sorted(l[prefix_len:] for l in lines if l.startswith(PREFIX))

  # override
  def RunTests(self, results, raw_logs_fh=None):
    # TODO(1384204): This step can take up to 3.5 seconds to execute when there
    # are a lot of tests.
    test_list = self.GetTestsForListing()
    grouped_tests = GroupTestsForShard(test_list)

    shard_list = list(range(len(grouped_tests)))
    shard_filter = self._test_instance.shard_filter
    if shard_filter:
      shard_list = [x for x in shard_list if x in shard_filter]

    if not shard_list:
      results_list = [
          base_test_result.BaseTestResult('Invalid shard filter',
                                          base_test_result.ResultType.UNKNOWN)
      ]
      test_run_results = base_test_result.TestRunResults()
      test_run_results.AddResults(results_list)
      results.append(test_run_results)
      return

    num_workers = ChooseNumOfWorkers(len(grouped_tests),
                                     self._test_instance.shards)
    if shard_filter:
      logging.warning('Running test shards: %s using %s concurrent process(es)',
                      ', '.join(str(x) for x in shard_list), num_workers)
    else:
      logging.warning(
          'Running tests with %d shard(s) using %s concurrent process(es).',
          len(grouped_tests), num_workers)

    with tempfile_ext.NamedTemporaryDirectory() as temp_dir:
      cmd_list = [[self._wrapper_path] for _ in shard_list]
      json_result_file_paths = [
          os.path.join(temp_dir, 'results%d.json' % i) for i in shard_list
      ]
      active_groups = [
          g for i, g in enumerate(grouped_tests) if i in shard_list
      ]
      jar_args_list = self._CreateJarArgsList(json_result_file_paths,
                                              active_groups, num_workers)
      if jar_args_list:
        for cmd, jar_args in zip(cmd_list, jar_args_list):
          cmd += ['--jar-args', '"%s"' % ' '.join(jar_args)]

      jvm_args = self._CreateJvmArgsList()
      if jvm_args:
        for cmd in cmd_list:
          cmd.extend(['--jvm-args', '"%s"' % ' '.join(jvm_args)])

      AddPropertiesJar(cmd_list, temp_dir, self._test_instance.resource_apk)

      show_logcat = logging.getLogger().isEnabledFor(logging.INFO)
      num_omitted_lines = 0
      failed_test_logs = {}
      log_lines = []
      current_test = None
      for line in _RunCommandsAndSerializeOutput(cmd_list, num_workers,
                                                 shard_list):
        if raw_logs_fh:
          raw_logs_fh.write(line)
        if show_logcat or not _LOGCAT_RE.match(line):
          sys.stdout.write(line)
        else:
          num_omitted_lines += 1

        # Collect log data between a test starting and the test failing.
        # There can be info after a test fails and before the next test starts
        # that we discard.
        test_start_match = _TEST_START_RE.match(line)
        if test_start_match:
          current_test = test_start_match.group(1)
          log_lines = [line]
        elif _TEST_FAILED_RE.match(line) and current_test:
          log_lines.append(line)
          failed_test_logs[current_test] = ''.join(log_lines)
          current_test = None
        else:
          log_lines.append(line)

      if num_omitted_lines > 0:
        logging.critical('%d log lines omitted.', num_omitted_lines)
      sys.stdout.flush()
      if raw_logs_fh:
        raw_logs_fh.flush()

      results_list = []
      failed_shards = []
      try:
        for i, json_file_path in enumerate(json_result_file_paths):
          with open(json_file_path, 'r') as f:
            parsed_results = json_results.ParseResultsFromJson(
                json.loads(f.read()))
            for r in parsed_results:
              if r.GetType() in _FAILURE_TYPES:
                r.SetLog(failed_test_logs.get(r.GetName().replace('#', '.'),
                                              ''))

            results_list += parsed_results
            if any(r for r in parsed_results if r.GetType() in _FAILURE_TYPES):
              failed_shards.append(shard_list[i])
      except IOError:
        # In the case of a failure in the JUnit or Robolectric test runner
        # the output json file may never be written.
        results_list = [
            base_test_result.BaseTestResult('Test Runner Failure',
                                            base_test_result.ResultType.UNKNOWN)
        ]

      if num_workers > 1 and failed_shards:
        for i in failed_shards:
          filt = ':'.join(grouped_tests[i])
          print(f'Test filter for failed shard {i}: --test-filter "{filt}"')

        print(
            f'{len(failed_shards)} shards had failing tests. To re-run only '
            f'these shards, use the above filter flags, or use: '
            f'--shard-filter', ','.join(str(x) for x in failed_shards))

      test_run_results = base_test_result.TestRunResults()
      test_run_results.AddResults(results_list)
      results.append(test_run_results)

  # override
  def TearDown(self):
    pass


def AddPropertiesJar(cmd_list, temp_dir, resource_apk):
  # Create properties file for Robolectric test runners so they can find the
  # binary resources.
  properties_jar_path = os.path.join(temp_dir, 'properties.jar')
  with zipfile.ZipFile(properties_jar_path, 'w') as z:
    z.writestr('com/android/tools/test_config.properties',
               'android_resource_apk=%s\n' % resource_apk)
    props = [
        'application = android.app.Application',
        'sdk = 28',
        ('shadows = org.chromium.testing.local.'
         'CustomShadowApplicationPackageManager'),
    ]
    z.writestr('robolectric.properties', '\n'.join(props))

  for cmd in cmd_list:
    cmd.extend(['--classpath', properties_jar_path])



def ChooseNumOfWorkers(num_jobs, num_workers=None):
  if num_workers is None:
    num_workers = max(1, multiprocessing.cpu_count() // 2)

  return min(num_workers, num_jobs)


def GroupTestsForShard(test_list):
  """Groups tests that will be run on each shard.

  Groups tests from the same SDK version. For a specific
  SDK version, groups tests from the same class together.

  Args:
    test_list: A list of the test names.

  Return:
    Returns a list of lists. Each list contains tests that should be run
    as a job together.
  """
  tests_by_sdk = defaultdict(set)
  for test in test_list:
    class_name, sdk_ver = _TEST_SDK_VERSION.match(test).groups()
    tests_by_sdk[sdk_ver].add((class_name, test))

  ret = []
  for tests_for_sdk in tests_by_sdk.values():
    tests_for_sdk = sorted(tests_for_sdk)
    test_count = 0
    # TODO(1458958): Group by classes instead of test names and
    # add --sdk-version as filter option. This will reduce filter verbiage.
    curr_tests = []

    for _, tests_from_class_tuple in itertools.groupby(tests_for_sdk,
                                                       lambda x: x[0]):
      temp_tests = [
          test.replace('#', '.') for _, test in tests_from_class_tuple
      ]
      test_count += len(temp_tests)
      curr_tests += temp_tests
      if test_count >= _MAX_TESTS_PER_JOB:
        ret.append(curr_tests)
        test_count = 0
        curr_tests = []

    ret.append(curr_tests)

  # Add an empty shard so that the test runner can throw a error from not
  # having any tests.
  if not ret:
    ret.append([])

  return ret


def _DumpJavaStacks(pid):
  jcmd = os.path.join(constants.JAVA_HOME, 'bin', 'jcmd')
  cmd = [jcmd, str(pid), 'Thread.print']
  result = subprocess.run(cmd,
                          check=False,
                          stdout=subprocess.PIPE,
                          encoding='utf8')
  if result.returncode:
    return 'Failed to dump stacks\n' + result.stdout
  return result.stdout


def _RunCommandsAndSerializeOutput(cmd_list, num_workers, shard_list):
  """Runs multiple commands in parallel and yields serialized output lines.

  Args:
    cmd_list: List of command lists to run.
    num_workers: The number of concurrent processes to run jobs in the
        shard_list.
    shard_list: Shard index of each command list.

  Raises:
    TimeoutError: If timeout is exceeded.

  Yields:
    Command output.
  """
  num_shards = len(shard_list)
  assert num_shards > 0
  temp_files = [None]  # First shard is streamed directly to stdout.
  for _ in range(num_shards - 1):
    temp_files.append(tempfile.TemporaryFile(mode='w+t', encoding='utf-8'))

  yield '\n'
  yield f'Shard {shard_list[0]} output:\n'

  timeout_dumps = {}

  def run_proc(cmd, idx):
    if idx == 0:
      s_out = subprocess.PIPE
      s_err = subprocess.STDOUT
    else:
      s_out = temp_files[idx]
      s_err = temp_files[idx]

    proc = cmd_helper.Popen(cmd, stdout=s_out, stderr=s_err)
    # Need to return process so that output can be displayed on stdout
    # in real time.
    if idx == 0:
      return proc

    try:
      proc.wait(timeout=(time.time() + _JOB_TIMEOUT))
    except subprocess.TimeoutExpired:
      timeout_dumps[idx] = _DumpJavaStacks(proc.pid)
      proc.kill()

    # Not needed, but keeps pylint happy.
    return None

  with ThreadPoolExecutor(max_workers=num_workers) as pool:
    futures = []
    for i, cmd in enumerate(cmd_list):
      futures.append(pool.submit(run_proc, cmd=cmd, idx=i))

    yield from _StreamFirstShardOutput(shard_list[0], futures[0].result(),
                                       time.time() + _JOB_TIMEOUT)

    for i, shard in enumerate(shard_list[1:]):
      # Shouldn't cause timeout as run_proc terminates the process with
      # a proc.wait().
      futures[i + 1].result()
      f = temp_files[i + 1]
      yield '\n'
      yield f'Shard {shard} output:\n'
      f.seek(0)
      for line in f.readlines():
        yield f'{shard:2}| {line}'
      f.close()

  # Output stacks
  if timeout_dumps:
    yield '\n'
    yield ('=' * 80) + '\n'
    yield '\nOne or mord shards timed out.\n'
    yield ('=' * 80) + '\n'
    for i, dump in timeout_dumps.items():
      yield f'Index of timed out shard: {shard_list[i]}\n'
      yield 'Thread dump:\n'
      yield dump
      yield '\n'

    raise cmd_helper.TimeoutError('Junit shards timed out.')


def _StreamFirstShardOutput(shard, shard_proc, deadline):
  # The following will be run from a thread to pump Shard 0 results, allowing
  # live output while allowing timeout.
  shard_queue = queue.Queue()

  def pump_stream_to_queue():
    for line in shard_proc.stdout:
      shard_queue.put(line)
    shard_queue.put(None)

  shard_0_pump = threading.Thread(target=pump_stream_to_queue)
  shard_0_pump.start()
  # Print the first process until timeout or completion.
  while shard_0_pump.is_alive():
    try:
      line = shard_queue.get(timeout=deadline - time.time())
      if line is None:
        break
      yield f'{shard:2}| {line}'
    except queue.Empty:
      if time.time() > deadline:
        break

  # Output any remaining output from a timed-out first shard.
  shard_0_pump.join()
  while not shard_queue.empty():
    line = shard_queue.get()
    if line:
      yield f'{shard:2}| {line}'
