#!/usr/bin/env vpython3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# pylint: disable=protected-access


import os
import unittest

from pylib.local.machine import local_machine_junit_test_run
from py_utils import tempfile_ext
from mock import patch  # pylint: disable=import-error


class LocalMachineJunitTestRunTests(unittest.TestCase):
  def testAddPropertiesJar(self):
    with tempfile_ext.NamedTemporaryDirectory() as temp_dir:
      apk = 'resource_apk'
      cmd_list = []
      local_machine_junit_test_run.AddPropertiesJar(cmd_list, temp_dir, apk)
      self.assertEqual(cmd_list, [])
      cmd_list = [['test1']]
      local_machine_junit_test_run.AddPropertiesJar(cmd_list, temp_dir, apk)
      self.assertEqual(
          cmd_list[0],
          ['test1', '--classpath',
           os.path.join(temp_dir, 'properties.jar')])
      cmd_list = [['test1'], ['test2']]
      local_machine_junit_test_run.AddPropertiesJar(cmd_list, temp_dir, apk)
      self.assertEqual(len(cmd_list[0]), 3)
      self.assertEqual(
          cmd_list[1],
          ['test2', '--classpath',
           os.path.join(temp_dir, 'properties.jar')])

  @patch('multiprocessing.cpu_count')
  def testChooseNumOfShards(self, mock_cpu_count):
    mock_cpu_count.return_value = 37
    # Tests set by num_cpus
    test_classes = [1] * 500
    shards = local_machine_junit_test_run.ChooseNumOfShards(test_classes)
    self.assertEqual(18, shards)

    # Tests using min_class per shards.
    test_classes = [1] * 20
    shards = local_machine_junit_test_run.ChooseNumOfShards(test_classes)
    self.assertEqual(2, shards)

    # Tests set by flag.
    shards = local_machine_junit_test_run.ChooseNumOfShards(test_classes, 18)
    self.assertEqual(18, shards)
    shards = local_machine_junit_test_run.ChooseNumOfShards(test_classes, 2)
    self.assertEqual(2, shards)

  def testGroupTestsForShard(self):
    test_classes = []
    results = local_machine_junit_test_run.GroupTestsForShard(1, test_classes)
    self.assertEqual(results, [[]])

    test_classes = ['dir/test.class'] * 5
    results = local_machine_junit_test_run.GroupTestsForShard(1, test_classes)
    self.assertEqual(results, [['dir.test*'] * 5])

    test_classes = ['dir/test.class'] * 5
    results = local_machine_junit_test_run.GroupTestsForShard(2, test_classes)
    ans_dict = [['dir.test*'] * 3, ['dir.test*'] * 2]
    self.assertEqual(results, ans_dict)

    test_classes = ['a10 warthog', 'b17', 'SR71']
    results = local_machine_junit_test_run.GroupTestsForShard(3, test_classes)
    ans_dict = [['a10 warthog'], ['b17'], ['SR71']]
    self.assertEqual(results, ans_dict)


if __name__ == '__main__':
  unittest.main()
