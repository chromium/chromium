#!/usr/bin/env vpython3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# pylint: disable=protected-access


import unittest

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



if __name__ == '__main__':
  unittest.main()
