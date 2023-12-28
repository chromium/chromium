# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import dataclasses
import json
import os
import re
import sys
from typing import Optional

from devil.android.tools import script_common
from devil.android.tools import webview_app
from pylib.base import base_test_result
from pylib.base import test_run
from pylib.local.machine import local_machine_junit_test_run as junitrun


_FAILURE_TYPES = (
    base_test_result.ResultType.FAIL,
    base_test_result.ResultType.CRASH,
    base_test_result.ResultType.TIMEOUT,
)

_TEST_START_RE = re.compile(r'.*=+ (\S+) STARTED: .*=+')
_TEST_END_RE = re.compile(r'.*=+ (\S+) ENDED: .*=+')
_JSON_RESULTS_RE = re.compile(
    r'.*D/LUCIResultReporter: JSON result for LUCI: (.*\.json)\b')


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

  # override
  def TestPackage(self):
    return self._test_instance.suite

  # override
  def GetTestsForListing(self):
    raise NotImplementedError

  # override
  def SetUp(self):
    if self._test_instance.use_webview_provider:
      device = script_common.GetDevices(
          requested_devices=None,
          denylist_file=None)[0]
      for apk in self._test_instance.additional_apks:
        device.Install(apk)
      self.webview_context = webview_app.UseWebViewProvider(
          device,
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

    cmd = [
        self._test_instance.tradefed_executable,
        'run',
        'commandAndExit',
        'cts',
        '-m',
        self.TestPackage(),
    ] + mode_args + [
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
      else:
        log_lines.append(line)
        if _TEST_END_RE.match(line) and current_test:
          per_test_logs[current_test] = ''.join(log_lines)
          current_test = None
        elif json_results_path_match := _JSON_RESULTS_RE.match(line):
          json_results_path = json_results_path_match.group(1)

    sys.stdout.flush()

    result_list = []
    if json_results_path:
      with open(json_results_path, 'r') as f:
        json_results = json.load(f)
        parsed_results = _ParseResultsFromJson(json_results)
      for r in parsed_results:
        if r.GetType() in _FAILURE_TYPES:
          r.SetLog(per_test_logs.get(r.GetName(), ''))

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
