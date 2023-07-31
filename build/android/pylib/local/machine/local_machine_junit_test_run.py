# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import dataclasses
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

# RegExp to detect logcat lines, e.g., 'I/AssetManager: not found'.
_LOGCAT_RE = re.compile(r' ?\d+\| (:?\d+\| )?[A-Z]/[\w\d_-]+:')

# Regex to detect start or failure of tests. Matches
# [ RUN      ] org.ui.ForeignSessionItemViewBinderUnitTest.test_phone[28]
# [ FAILED|CRASHED|TIMEOUT ] org.ui.ForeignBinderUnitTest.test_phone[28] (56 ms)
_TEST_START_RE = re.compile(r'.*\[\s+RUN\s+\]\s(.*)')
_TEST_FAILED_RE = re.compile(r'.*\[\s+(?:FAILED|CRASHED|TIMEOUT)\s+\]')

# Regex that matches a test name, and optionally matches the sdk version e.g.:
# org.chromium.default_browser_promo.PromoUtilsTest#testNoPromo[28]'
_TEST_SDK_VERSION = re.compile(r'(.*\.\w+)#\w+(?:\[(\d+)\])?')


@dataclasses.dataclass
class _Job:
  shard_id: int
  cmd: str
  timeout: int
  gtest_filter: str
  results_json_path: str


class LocalMachineJunitTestRun(test_run.TestRun):
  # override
  def TestPackage(self):
    return self._test_instance.suite

  # override
  def SetUp(self):
    pass

  def _GetFilterArgs(self):
    ret = []
    for test_filter in self._test_instance.test_filters:
      ret += ['-gtest-filter', test_filter]

    if self._test_instance.package_filter:
      ret += ['-package-filter', self._test_instance.package_filter]
    if self._test_instance.runner_filter:
      ret += ['-runner-filter', self._test_instance.runner_filter]

    return ret

  def _CreatePropertiesJar(self, temp_dir):
    # Create properties file for Robolectric test runners so they can find the
    # binary resources.
    properties_jar_path = os.path.join(temp_dir, 'properties.jar')
    resource_apk = self._test_instance.resource_apk
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
    return properties_jar_path

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

  def _ChooseNumWorkers(self, num_jobs):
    if self._test_instance.debug_socket:
      num_workers = 1
    elif self._test_instance.shards is not None:
      num_workers = self._test_instance.shards
    else:
      num_workers = max(1, multiprocessing.cpu_count() // 2)
    return min(num_workers, num_jobs)

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
      cmd += ['--classpath', self._CreatePropertiesJar(temp_dir)]

      try:
        lines = subprocess.check_output(cmd, encoding='utf8').splitlines()
      except subprocess.CalledProcessError:
        # Will get an error later on from testrunner from having no tests.
        return []

    PREFIX = '#TEST# '
    prefix_len = len(PREFIX)
    # Filter log messages other than test names (Robolectric logs to stdout).
    return sorted(l[prefix_len:] for l in lines if l.startswith(PREFIX))

  def _MakeJob(self, shard_id, temp_dir, tests, properties_jar_path):
    results_json_path = os.path.join(temp_dir, f'results{shard_id}.json')
    gtest_filter = ':'.join(tests)

    cmd = [self._wrapper_path]
    cmd += ['--jvm-args', '"%s"' % ' '.join(self._CreateJvmArgsList())]
    cmd += ['--classpath', properties_jar_path]
    cmd += ['-json-results-file', results_json_path]
    cmd += ['-gtest-filter', gtest_filter]

    # 20 seconds for process init + 2 seconds per test.
    timeout = 20 + len(tests) * 2
    return _Job(shard_id=shard_id,
                cmd=cmd,
                timeout=timeout,
                gtest_filter=gtest_filter,
                results_json_path=results_json_path)

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

    num_workers = self._ChooseNumWorkers(len(shard_list))
    if shard_filter:
      logging.warning('Running test shards: %s using %s concurrent process(es)',
                      ', '.join(str(x) for x in shard_list), num_workers)
    else:
      logging.warning(
          'Running tests with %d shard(s) using %s concurrent process(es).',
          len(shard_list), num_workers)

    with tempfile_ext.NamedTemporaryDirectory() as temp_dir:
      properties_jar_path = self._CreatePropertiesJar(temp_dir)
      jobs = [
          self._MakeJob(i, temp_dir, grouped_tests[i], properties_jar_path)
          for i in shard_list
      ]

      show_logcat = logging.getLogger().isEnabledFor(logging.INFO)
      num_omitted_lines = 0
      failed_test_logs = {}
      log_lines = []
      current_test = None
      for line in _RunCommandsAndSerializeOutput(jobs, num_workers):
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
      failed_jobs = []
      try:
        for job in jobs:
          with open(job.results_json_path, 'r') as f:
            parsed_results = json_results.ParseResultsFromJson(
                json.loads(f.read()))
          has_failed = False
          for r in parsed_results:
            if r.GetType() in _FAILURE_TYPES:
              has_failed = True
              r.SetLog(failed_test_logs.get(r.GetName().replace('#', '.'), ''))

          results_list += parsed_results
          if has_failed:
            failed_jobs.append(job)
      except IOError:
        # In the case of a failure in the JUnit or Robolectric test runner
        # the output json file may never be written.
        results_list = [
            base_test_result.BaseTestResult('Test Runner Failure',
                                            base_test_result.ResultType.UNKNOWN)
        ]

      if num_workers > 1 and failed_jobs:
        for job in failed_jobs:
          print(f'Test filter for failed shard {job.shard_id}: '
                f'--test-filter "{job.gtest_filter}"')

        print(
            f'{len(failed_jobs)} shards had failing tests. To re-run only '
            f'these shards, use the above filter flags, or use: '
            f'--shard-filter', ','.join(str(j.shard_id) for j in failed_jobs))

      test_run_results = base_test_result.TestRunResults()
      test_run_results.AddResults(results_list)
      results.append(test_run_results)

  # override
  def TearDown(self):
    pass


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


def _RunCommandsAndSerializeOutput(jobs, num_workers):
  """Runs multiple commands in parallel and yields serialized output lines.

  Raises:
    TimeoutError: If timeout is exceeded.

  Yields:
    Command output.
  """
  assert jobs
  temp_files = [None]  # First shard is streamed directly to stdout.
  for _ in range(len(jobs) - 1):
    temp_files.append(tempfile.TemporaryFile(mode='w+t', encoding='utf-8'))

  yield '\n'
  yield f'Shard {jobs[0].shard_id} output:\n'

  timeout_dumps = {}

  def run_proc(idx):
    if idx == 0:
      s_out = subprocess.PIPE
      s_err = subprocess.STDOUT
    else:
      s_out = temp_files[idx]
      s_err = temp_files[idx]

    job = jobs[idx]
    proc = cmd_helper.Popen(job.cmd, stdout=s_out, stderr=s_err)
    # Need to return process so that output can be displayed on stdout
    # in real time.
    if idx == 0:
      return proc

    try:
      proc.wait(timeout=job.timeout)
    except subprocess.TimeoutExpired:
      timeout_dumps[idx] = _DumpJavaStacks(proc.pid)
      proc.kill()

    # Not needed, but keeps pylint happy.
    return None

  with ThreadPoolExecutor(max_workers=num_workers) as pool:
    futures = [pool.submit(run_proc, idx=i) for i in range(len(jobs))]

    yield from _StreamFirstShardOutput(jobs[0], futures[0].result())

    for i, job in enumerate(jobs[1:], 1):
      shard_id = job.shard_id
      # Shouldn't cause timeout as run_proc terminates the process with
      # a proc.wait().
      futures[i].result()
      f = temp_files[i]
      yield '\n'
      yield f'Shard {shard_id} output:\n'
      f.seek(0)
      for line in f.readlines():
        yield f'{shard_id:2}| {line}'
      f.close()

  # Output stacks
  if timeout_dumps:
    yield '\n'
    yield ('=' * 80) + '\n'
    yield '\nOne or mord shards timed out.\n'
    yield ('=' * 80) + '\n'
    for i, dump in sorted(timeout_dumps.items()):
      job = jobs[i]
      yield f'Shard {job.shard_id} timed out after {job.timeout} seconds.\n'
      yield 'Thread dump:\n'
      yield dump
      yield '\n'

    raise cmd_helper.TimeoutError('Junit shards timed out.')


def _StreamFirstShardOutput(job, shard_proc):
  shard_id = job.shard_id
  # The following will be run from a thread to pump Shard 0 results, allowing
  # live output while allowing timeout.
  shard_queue = queue.Queue()

  def pump_stream_to_queue():
    for line in shard_proc.stdout:
      shard_queue.put(line)
    shard_queue.put(None)

  shard_0_pump = threading.Thread(target=pump_stream_to_queue)
  shard_0_pump.start()
  deadline = time.time() + job.timeout
  # Print the first process until timeout or completion.
  while shard_0_pump.is_alive():
    try:
      line = shard_queue.get(timeout=max(0, deadline - time.time()))
      if line is None:
        break
      yield f'{shard_id:2}| {line}'
    except queue.Empty:
      if time.time() > deadline:
        break

  # Output any remaining output from a timed-out first shard.
  shard_0_pump.join()
  while not shard_queue.empty():
    line = shard_queue.get()
    if line:
      yield f'{shard_id:2}| {line}'
