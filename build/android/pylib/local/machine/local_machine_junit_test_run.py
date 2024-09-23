# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import dataclasses
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


@dataclasses.dataclass
class _TestGroup:
  config: str
  methods_by_class: dict


@dataclasses.dataclass
class _Job:
  shard_id: int
  cmd: str
  timeout: int
  json_config: dict
  json_results_path: str


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
      ret += ['--gtest-filter', test_filter]

    if self._test_instance.package_filter:
      ret += ['--package-filter', self._test_instance.package_filter]
    if self._test_instance.runner_filter:
      ret += ['--runner-filter', self._test_instance.runner_filter]

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

  def _CreateJvmArgsList(self, for_listing=False, allow_debugging=True):
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
    if self._test_instance.debug_socket and allow_debugging:
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
                                         'third_party', 'jacoco', 'cipd', 'lib',
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

  def _QueryTestJsonConfig(self,
                           temp_dir,
                           allow_debugging=True,
                           enable_shadow_allowlist=False):
    json_config_path = os.path.join(temp_dir, 'main_test_config.json')
    cmd = [self._wrapper_path]
    # Allow debugging of test listing when run as:
    # "--wait-for-java-debugger --list-tests"
    jvm_args = self._CreateJvmArgsList(for_listing=True,
                                       allow_debugging=allow_debugging)
    if jvm_args:
      cmd += ['--jvm-args', '"%s"' % ' '.join(jvm_args)]
    cmd += ['--classpath', self._CreatePropertiesJar(temp_dir)]
    cmd += ['--list-tests', '--json-config', json_config_path]
    if enable_shadow_allowlist and self._test_instance.shadows_allowlist:
      cmd += ['--shadows-allowlist', self._test_instance.shadows_allowlist]
    cmd += self._GetFilterArgs()
    subprocess.run(cmd, check=True)
    with open(json_config_path) as f:
      return json.load(f)

  def _MakeJob(self, shard_id, temp_dir, test_group, properties_jar_path,
               json_config):
    json_results_path = os.path.join(temp_dir, f'results{shard_id}.json')
    job_json_config_path = os.path.join(temp_dir, f'config{shard_id}.json')
    job_json_config = json_config.copy()
    job_json_config['configs'] = {
        test_group.config: test_group.methods_by_class
    }
    with open(job_json_config_path, 'w') as f:
      json.dump(job_json_config, f)

    cmd = [self._wrapper_path]
    cmd += ['--jvm-args', '"%s"' % ' '.join(self._CreateJvmArgsList())]
    cmd += ['--classpath', properties_jar_path]
    cmd += ['--json-results', json_results_path]
    cmd += ['--json-config', job_json_config_path]

    if self._test_instance.debug_socket:
      timeout = 999999
    else:
      # 20 seconds for process init,
      # 5 seconds per class,
      # 3 seconds per method.
      num_classes = len(test_group.methods_by_class)
      num_tests = sum(len(x) for x in test_group.methods_by_class.values())
      timeout = 20 + 5 * num_classes + num_tests * 3
    return _Job(shard_id=shard_id,
                cmd=cmd,
                timeout=timeout,
                json_config=job_json_config,
                json_results_path=json_results_path)

  #override
  def GetTestsForListing(self):
    with tempfile_ext.NamedTemporaryDirectory() as temp_dir:
      json_config = self._QueryTestJsonConfig(temp_dir)
      ret = []
      for config in json_config['configs'].values():
        for class_name, methods in config.items():
          ret.extend(f'{class_name}.{method}' for method in methods)
      ret.sort()
      return ret

  # override
  def RunTests(self, results, raw_logs_fh=None):
    with tempfile_ext.NamedTemporaryDirectory() as temp_dir:
      self._RunTestsInternal(temp_dir, results, raw_logs_fh)

  def _RunTestsInternal(self, temp_dir, results, raw_logs_fh):
    if self._test_instance.json_config:
      with open(self._test_instance.json_config) as f:
        json_config = json.load(f)
    else:
      # TODO(crbug.com/40878339): This step can take 3-4 seconds for
      # chrome_junit_tests.
      try:
        json_config = self._QueryTestJsonConfig(temp_dir,
                                                allow_debugging=False,
                                                enable_shadow_allowlist=True)
      except subprocess.CalledProcessError:
        results.append(_MakeUnknownFailureResult('Filter matched no tests'))
        return
    test_groups = GroupTests(json_config, _MAX_TESTS_PER_JOB)

    shard_list = list(range(len(test_groups)))
    shard_filter = self._test_instance.shard_filter
    if shard_filter:
      shard_list = [x for x in shard_list if x in shard_filter]

    if not shard_list:
      results.append(_MakeUnknownFailureResult('Invalid shard filter'))
      return

    num_workers = self._ChooseNumWorkers(len(shard_list))
    if shard_filter:
      logging.warning('Running test shards: %s using %s concurrent process(es)',
                      ', '.join(str(x) for x in shard_list), num_workers)
    else:
      logging.warning(
          'Running tests with %d shard(s) using %s concurrent process(es).',
          len(shard_list), num_workers)

    properties_jar_path = self._CreatePropertiesJar(temp_dir)
    jobs = [
        self._MakeJob(i, temp_dir, test_groups[i], properties_jar_path,
                      json_config) for i in shard_list
    ]

    show_logcat = logging.getLogger().isEnabledFor(logging.INFO)
    num_omitted_lines = 0
    failed_test_logs = {}
    log_lines = []
    current_test = None
    for line in RunCommandsAndSerializeOutput(jobs, num_workers):
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

    results_list = []
    failed_jobs = []
    try:
      for job in jobs:
        with open(job.json_results_path, 'r') as f:
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

    if failed_jobs:
      for job in failed_jobs:
        print(f'To re-run failed shard {job.shard_id}, use --json-config '
              'config.json, where config.json contains:')
        print(json.dumps(job.json_config, indent=2))
        print()

      print(
          f'To re-run the {len(failed_jobs)} failed shard(s), use: '
          f'--shards {num_workers} --shard-filter',
          ','.join(str(j.shard_id) for j in failed_jobs))

    test_run_results = base_test_result.TestRunResults()
    test_run_results.AddResults(results_list)
    results.append(test_run_results)

  # override
  def TearDown(self):
    pass


def GroupTests(json_config, max_per_job):
  """Groups tests that will be run on each shard.

  Args:
    json_config: The result from _QueryTestJsonConfig().
    max_per_job: Stop adding tests to a group once this limit has been passed.

  Return:
    Returns a list of _TestGroup.
  """
  ret = []
  for config, methods_by_class in json_config['configs'].items():
    size = 0
    group = {}
    for class_name, methods in methods_by_class.items():
      # There is some per-class overhead, so do not splits tests from one class
      # across multiple shards (unless configs differ).
      group[class_name] = methods
      size += len(methods)
      if size >= max_per_job:
        ret.append(_TestGroup(config, group))
        group = {}
        size = 0

    if group:
      ret.append(_TestGroup(config, group))

  # Put largest shards first to prevent long shards from being scheduled right
  # at the end.
  ret.sort(key=lambda x: -len(x.methods_by_class))
  return ret


def _MakeUnknownFailureResult(message):
  results_list = [
      base_test_result.BaseTestResult(message,
                                      base_test_result.ResultType.UNKNOWN)
  ]
  test_run_results = base_test_result.TestRunResults()
  test_run_results.AddResults(results_list)
  return test_run_results


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


def RunCommandsAndSerializeOutput(jobs, num_workers):
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
    proc = cmd_helper.Popen(job.cmd, stdout=s_out, stderr=s_err,
                            env=getattr(job, 'env', None))
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
    yield '\nOne or more shards timed out.\n'
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
