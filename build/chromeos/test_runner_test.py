#!/usr/bin/env vpython
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import os
import shutil
import sys
import tempfile
import unittest

# The following non-std imports are fetched via vpython. See the list at
# //.vpython
import mock  # pylint: disable=import-error
from parameterized import parameterized  # pylint: disable=import-error

import test_runner

_TAST_TEST_RESULTS_JSON = {
    "name": "ui.ChromeLogin",
    "errors": None,
    "start": "2020-01-01T15:41:30.799228462-08:00",
    "end": "2020-01-01T15:41:53.318914698-08:00",
    "skipReason": ""
}


class TestRunnerTest(unittest.TestCase):

  def setUp(self):
    self._tmp_dir = tempfile.mkdtemp()
    self.mock_rdb = mock.patch.object(
        test_runner.result_sink, 'TryInitClient', return_value=None)
    self.mock_rdb.start()

  def tearDown(self):
    shutil.rmtree(self._tmp_dir, ignore_errors=True)
    self.mock_rdb.stop()

  def get_common_tast_args(self, use_vm):
    return [
        'script_name',
        'tast',
        '--suite-name=chrome_all_tast_tests',
        '--board=eve',
        '--flash',
        '--path-to-outdir=out_eve/Release',
        '--logs-dir=%s' % self._tmp_dir,
        '--use-vm' if use_vm else '--device=localhost:2222',
    ]

  def get_common_tast_expectations(self, use_vm, is_lacros=False):
    expectation = [
        test_runner.CROS_RUN_TEST_PATH,
        '--board',
        'eve',
        '--cache-dir',
        test_runner.DEFAULT_CROS_CACHE,
        '--results-dest-dir',
        '%s/system_logs' % self._tmp_dir,
        '--flash',
        '--build-dir',
        'out_eve/Release',
        '--results-dir',
        self._tmp_dir,
        '--tast-total-shards=1',
        '--tast-shard-index=0',
    ]
    expectation.extend(['--start', '--copy-on-write']
                       if use_vm else ['--device', 'localhost:2222'])
    for p in test_runner.SYSTEM_LOG_LOCATIONS:
      expectation.extend(['--results-src', p])

    if not is_lacros:
      expectation += [
          '--mount',
          '--deploy',
          '--nostrip',
      ]
    return expectation

  @parameterized.expand([
      [True],
      [False],
  ])
  def test_gtest(self, use_vm):
    """Tests running a gtest."""
    fd_mock = mock.mock_open()

    args = [
        'script_name',
        'vm-test',
        '--test-exe=out_eve/Release/base_unittests',
        '--board=eve',
        '--path-to-outdir=out_eve/Release',
        '--use-vm' if use_vm else '--device=localhost:2222',
    ]
    with mock.patch.object(sys, 'argv', args),\
         mock.patch.object(test_runner.subprocess, 'Popen') as mock_popen,\
         mock.patch.object(os, 'fdopen', fd_mock),\
         mock.patch.object(os, 'remove') as mock_remove,\
         mock.patch.object(tempfile, 'mkstemp',
            return_value=(3, 'out_eve/Release/device_script.sh')),\
         mock.patch.object(os, 'fchmod'):
      mock_popen.return_value.returncode = 0

      test_runner.main()
      self.assertEqual(1, mock_popen.call_count)
      expected_cmd = [
          test_runner.CROS_RUN_TEST_PATH, '--board', 'eve', '--cache-dir',
          test_runner.DEFAULT_CROS_CACHE, '--as-chronos', '--remote-cmd',
          '--cwd', 'out_eve/Release', '--files',
          'out_eve/Release/device_script.sh'
      ]
      expected_cmd.extend(['--start', '--copy-on-write']
                          if use_vm else ['--device', 'localhost:2222'])
      expected_cmd.extend(['--', './device_script.sh'])
      self.assertItemsEqual(expected_cmd, mock_popen.call_args[0][0])

      fd_mock().write.assert_called_once_with(
          '#!/bin/sh\nexport HOME=/usr/local/tmp\n'
          'export TMPDIR=/usr/local/tmp\n'
          'LD_LIBRARY_PATH=./ ./out_eve/Release/base_unittests '
          '--test-launcher-shard-index=0 --test-launcher-total-shards=1\n')
      mock_remove.assert_called_once_with('out_eve/Release/device_script.sh')

  @parameterized.expand([
      [True],
      [False],
  ])
  def test_tast(self, use_vm):
    """Tests running a tast tests."""
    with open(os.path.join(self._tmp_dir, 'streamed_results.jsonl'), 'w') as f:
      json.dump(_TAST_TEST_RESULTS_JSON, f)

    args = self.get_common_tast_args(use_vm) + [
        '-t=ui.ChromeLogin',
    ]
    with mock.patch.object(sys, 'argv', args),\
         mock.patch.object(test_runner.subprocess, 'Popen') as mock_popen:
      mock_popen.return_value.returncode = 0

      test_runner.main()
      expected_cmd = self.get_common_tast_expectations(use_vm) + [
          '--tast', 'ui.ChromeLogin'
      ]

      self.assertItemsEqual(expected_cmd, mock_popen.call_args[0][0])

  @parameterized.expand([
      [True],
      [False],
  ])
  def test_tast_attr_expr(self, use_vm):
    """Tests running a tast tests specified by an attribute expression."""
    with open(os.path.join(self._tmp_dir, 'streamed_results.jsonl'), 'w') as f:
      json.dump(_TAST_TEST_RESULTS_JSON, f)

    args = self.get_common_tast_args(use_vm) + [
        '--attr-expr=( "group:mainline" && "dep:chrome" && !informational)',
    ]
    with mock.patch.object(sys, 'argv', args),\
         mock.patch.object(test_runner.subprocess, 'Popen') as mock_popen:
      mock_popen.return_value.returncode = 0

      test_runner.main()
      expected_cmd = self.get_common_tast_expectations(use_vm) + [
          '--tast=( "group:mainline" && "dep:chrome" && !informational)',
      ]

      self.assertItemsEqual(expected_cmd, mock_popen.call_args[0][0])

  @parameterized.expand([
      [True],
      [False],
  ])
  def test_tast_lacros(self, use_vm):
    """Tests running a tast tests for Lacros."""
    with open(os.path.join(self._tmp_dir, 'streamed_results.jsonl'), 'w') as f:
      json.dump(_TAST_TEST_RESULTS_JSON, f)

    args = self.get_common_tast_args(use_vm) + [
        '-t=lacros.Basic',
        '--deploy-lacros',
    ]

    with mock.patch.object(sys, 'argv', args),\
         mock.patch.object(test_runner.subprocess, 'Popen') as mock_popen:
      mock_popen.return_value.returncode = 0

      test_runner.main()
      expected_cmd = self.get_common_tast_expectations(
          use_vm, is_lacros=True) + [
              '--tast',
              'lacros.Basic',
              '--deploy-lacros',
          ]

      self.assertItemsEqual(expected_cmd, mock_popen.call_args[0][0])

  @parameterized.expand([
      [True],
      [False],
  ])
  def test_tast_with_vars(self, use_vm):
    """Tests running a tast tests with runtime variables."""
    with open(os.path.join(self._tmp_dir, 'streamed_results.jsonl'), 'w') as f:
      json.dump(_TAST_TEST_RESULTS_JSON, f)

    args = self.get_common_tast_args(use_vm) + [
        '-t=ui.ChromeLogin',
        '--tast-var=key=value',
    ]
    with mock.patch.object(sys, 'argv', args),\
         mock.patch.object(test_runner.subprocess, 'Popen') as mock_popen:
      mock_popen.return_value.returncode = 0
      test_runner.main()
      expected_cmd = self.get_common_tast_expectations(use_vm) + [
          '--tast', 'ui.ChromeLogin', '--tast-var', 'key=value'
      ]

      self.assertItemsEqual(expected_cmd, mock_popen.call_args[0][0])


if __name__ == '__main__':
  unittest.main()
