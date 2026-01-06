#!/usr/bin/env vpython3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# pylint: disable=protected-access

import unittest
from unittest.mock import MagicMock, patch

from pylib.local.machine import local_machine_junit_test_run

class LocalMachineJunitTestRunTests(unittest.TestCase):
  def testGroupTests(self):
    # Tests grouping tests when classes_per_job is exceeded.
    # All tests are from same class so should be in a single job.
    MAX_TESTS_PER_JOB = 3
    json_config = {
        'configs': {
            'config1': {
                'class1': ['m1', 'm2'],
                'class2': ['m1', 'm2'],
                'class3': ['m1', 'm2'],
            },
            'config2': {
                'class1': ['m3', 'm4', 'm5'],
            },
        }
    }
    actual = local_machine_junit_test_run.GroupTests(json_config,
                                                     MAX_TESTS_PER_JOB)

    expected = [
        local_machine_junit_test_run._TestGroup(config='config1',
                                                methods_by_class={
                                                    'class1': ['m1', 'm2'],
                                                    'class2': ['m1', 'm2']
                                                }),
        local_machine_junit_test_run._TestGroup(
            config='config1', methods_by_class={'class3': ['m1', 'm2']}),
        local_machine_junit_test_run._TestGroup(
            config='config2', methods_by_class={'class1': ['m3', 'm4', 'm5']}),
    ]
    self.assertEqual(expected, actual)

  def testRunCommandsAndSerializeOutputQuiet(self):
    job = MagicMock()
    job.shard_id = 0
    job.cmd = ['echo', 'hello']
    job.timeout = 10

    with patch('devil.utils.cmd_helper.Popen') as mock_popen:
      mock_proc = MagicMock()
      mock_proc.stdout = ['line1\n', 'line2\n']
      mock_popen.return_value = mock_proc

      jobs = [job]
      num_workers = 1

      # Test with quiet=False
      output_normal = list(
          local_machine_junit_test_run.RunCommandsAndSerializeOutput(
              jobs, num_workers, quiet=False))
      self.assertTrue(any('Shard 0 output:' in line for line in output_normal))
      self.assertTrue(any(' 0| line1' in line for line in output_normal))

      # Test with quiet=True
      output_quiet = list(
          local_machine_junit_test_run.RunCommandsAndSerializeOutput(
              jobs, num_workers, quiet=True))
      self.assertFalse(any('Shard 0 output:' in line for line in output_quiet))
      self.assertFalse(any(' 0| line1' in line for line in output_quiet))
      self.assertTrue(any(line == 'line1\n' for line in output_quiet))

  def testRegexes(self):
    self.assertTrue(
        local_machine_junit_test_run._TEST_START_RE.match('[ RUN      ] test'))
    self.assertTrue(
        local_machine_junit_test_run._TEST_START_RE.match(
            ' 0| [ RUN      ] test'))
    self.assertTrue(
        local_machine_junit_test_run._TEST_FAILED_RE.match('[  FAILED  ] test'))
    self.assertTrue(
        local_machine_junit_test_run._TEST_FAILED_RE.match(
            ' 0| [  FAILED  ] test'))
    self.assertTrue(
        local_machine_junit_test_run._TEST_FINISHED_RE.match(
            '[       OK ] test'))
    self.assertTrue(
        local_machine_junit_test_run._TEST_FINISHED_RE.match(
            ' 0| [       OK ] test'))
    self.assertTrue(
        local_machine_junit_test_run._TEST_FINISHED_RE.match(
            '[  SKIPPED ] test'))
    self.assertTrue(
        local_machine_junit_test_run._TEST_FINISHED_RE.match(
            ' 0| [  SKIPPED ] test'))

if __name__ == '__main__':
  unittest.main()
