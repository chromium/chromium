# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections
import contextlib
import dataclasses
import json
import logging
import os
import re
import sys
import time
from typing import Optional

from devil.android import logcat_monitor
from devil.android.tools import script_common
from devil.android.tools import webview_app
from pylib.base import base_test_result
from pylib.base import test_run
from pylib.local.machine import local_machine_junit_test_run as junitrun
from pylib.symbols import stack_symbolizer


_FAILURE_TYPES = (
    base_test_result.ResultType.FAIL,
    base_test_result.ResultType.CRASH,
    base_test_result.ResultType.TIMEOUT,
)

_TEST_START_RE = re.compile(r'.*=+ (\S+) STARTED: .*=+')
_TEST_END_RE = re.compile(r'.*=+ (\S+) ENDED: .*=+')
_JSON_RESULTS_RE = re.compile(
    r'.*D/LUCIResultReporter: JSON result for LUCI: (.*\.json)\b')

LOGCAT_FILTERS = [
  'chromium:v',
  'cr_*:v',
  'DEBUG:I',
  'StrictMode:D',
]

@dataclasses.dataclass
class _Job:
  shard_id: int
  cmd: str
  timeout: int
  env: Optional[dict] = None


class LocalMachineHostsideTestRun(test_run.TestRun):
  def __init__(self, env, test_instance):
    super().__init__(env, test_instance)
    self.webview_context = None
    self.device = None
    self.test_name_to_logcat = collections.defaultdict(collections.deque)

  # override
  def TestPackage(self):
    return self._test_instance.suite

  # override
  def GetTestsForListing(self):
    raise NotImplementedError

  # override
  def SetUp(self):
    if self._test_instance.use_webview_provider:
      self.device = script_common.GetDevices(
          requested_devices=None,
          denylist_file=None)[0]
      for apk in self._test_instance.additional_apks:
        self.device.Install(apk)
      self.webview_context = webview_app.UseWebViewProvider(
          self.device,
          self._test_instance.use_webview_provider)
      # Pylint is not smart enough to realize that this field has
      # an __enter__ method, and will complain loudly.
      # pylint: disable=no-member
      self.webview_context.__enter__()
      # pylint: enable=no-member

  def _MakeJob(self, shard_id):
    if self._test_instance.instant_mode:
      mode_args = [
          '--module-parameter',
          'INSTANT_APP',
      ]
    else:
      mode_args = [
          '--exclude-filter',
          f'{self.TestPackage()}[instant]',
      ]

    filter_args = []
    for combined_filter in self._test_instance.test_filters:
      pattern_groups = combined_filter.split('-')
      negative_pattern = pattern_groups[1] if len(pattern_groups) > 1 else None
      positive_pattern = pattern_groups[0]
      if negative_pattern:
        for exclude_filter in negative_pattern.split(':'):
          filter_args.extend([
              '--exclude-filter',
              self.TestPackage()
              + '[instant]' * self._test_instance.instant_mode
              + ' ' + '#'.join(exclude_filter.rsplit('.', 1)),
          ])
      if positive_pattern:
        for include_filter in positive_pattern.split(':'):
          filter_args.extend([
              '--include-filter',
              self.TestPackage()
              + '[instant]' * self._test_instance.instant_mode
              + ' ' + '#'.join(include_filter.rsplit('.', 1)),
          ])

    cmd = [
        self._test_instance.tradefed_executable,
        'run',
        'commandAndExit',
        'cts',
        '-m',
        self.TestPackage(),
    ] + mode_args + filter_args + [
        '--retry-strategy',
        'RETRY_ANY_FAILURE',
        '--max-testcase-run-count',
        str(self._test_instance.max_tries),
        '--template:map',
        'reporters=../../build/android/pylib/local/machine/'
        'local_machine_hostside_tradefed_config.xml',
    ]

    return _Job(
        shard_id=shard_id,
        cmd=cmd,
        timeout=600,
        env=dict(os.environ,
                 PATH=':'.join([
                   os.getenv('PATH'),
                   self._test_instance.aapt_path,
                   self._test_instance.adb_path]),
                 CTS_ROOT=os.path.join(
                   os.path.dirname(self._test_instance.tradefed_executable),
                   os.pardir,
                   os.pardir
                 )
        )
    )

  # override
  def RunTests(self, results, raw_logs_fh=None):
    job = self._MakeJob(0)

    per_test_logs = {}
    log_lines = []
    current_test = None
    json_results_path = None
    archive_logcat = None
    for line in junitrun.RunCommandsAndSerializeOutput([job], 1):
      if raw_logs_fh:
        raw_logs_fh.write(line)
      sys.stdout.write(line)

      # Collect log data between a test starting and the test failing.
      # There can be info after a test fails and before the next test starts
      # that we discard.
      if test_start_match := _TEST_START_RE.match(line):
        current_test = test_start_match.group(1)
        log_lines = [line]
        if archive_logcat is not None:
          archive_logcat.__exit__(None, None, None)
        archive_logcat = self._ArchiveLogcat(self.device, current_test)
        # Pylint is not smart enough to realize that this field has
        # an __enter__ method, and will complain loudly.
        # pylint: disable=no-member
        archive_logcat.__enter__()
        # pylint: enable=no-member
      else:
        log_lines.append(line)
        if _TEST_END_RE.match(line) and current_test:
          per_test_logs[current_test] = ''.join(log_lines)
          archive_logcat.__exit__(None, None, None)
          archive_logcat = None
          current_test = None
        elif json_results_path_match := _JSON_RESULTS_RE.match(line):
          json_results_path = json_results_path_match.group(1)

    sys.stdout.flush()
    if archive_logcat is not None:
      # Pylint is not smart enough to realize that this field has
      # an __exit__ method, and will complain loudly.
      # pylint: disable=no-member
      archive_logcat.__exit__(None, None, None)
      # pylint: enable=no-member

    result_list = []
    if json_results_path:
      with open(json_results_path, 'r') as f:
        json_results = json.load(f)
        parsed_results = _ParseResultsFromJson(json_results)
      for r in parsed_results:
        if r.GetType() in _FAILURE_TYPES:
          r.SetLog(per_test_logs.get(r.GetName(), ''))
      attempt_counter = collections.Counter(
          result.GetName() for result in parsed_results
      )
      for result in parsed_results:
        test_name = result.GetName()
        logcat_deque = self.test_name_to_logcat[test_name]
        if attempt_counter[test_name] == len(logcat_deque):
          # Set logcat link in FIFO order in case of multiple test attempts
          result.SetLink('logcat', logcat_deque.popleft())
          attempt_counter[test_name] -= 1

      result_list += parsed_results
    else:
      # In the case of a failure in the test runner
      # the output json file may never be written.
      result_list = [
          base_test_result.BaseTestResult(
              'Test Runner Failure',
              base_test_result.ResultType.UNKNOWN)
      ]

    test_run_results = base_test_result.TestRunResults()
    test_run_results.AddResults(result_list)
    results.append(test_run_results)

  # override
  def TearDown(self):
    if self._test_instance.use_webview_provider:
      # Pylint is not smart enough to realize that this field has
      # an __exit__ method, and will complain loudly.
      # pylint: disable=no-member
      self.webview_context.__exit__(*sys.exc_info())
      # pylint: enable=no-member

  @contextlib.contextmanager
  def _ArchiveLogcat(self, device, test_name):
    stream_name = 'logcat_%s_shard%s_%s_%s' % (
        test_name.replace('#', '.'), 0,
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
              filter_specs=LOGCAT_FILTERS,
              output_file=logcat_file.name,
              check_error=False) as logmon:
              yield logcat_file
    finally:
      if logmon:
        logmon.Close()
      if logcat_file and logcat_file.Link():
        logging.critical('Logcat saved to %s', logcat_file.Link())
        self.test_name_to_logcat[test_name].append(logcat_file.Link())

def _ParseResultsFromJson(json_results):
  result_list = []
  result_list.extend([
      base_test_result.BaseTestResult(
          tr['testId'],
          getattr(
              base_test_result.ResultType,
              tr['status'],
              base_test_result.ResultType.UNKNOWN
          ),
          duration=int(tr['duration'] * 1000),
          failure_reason=tr.get('failureReason'),
      )
      for tr in json_results['tr']
  ])
  return result_list
