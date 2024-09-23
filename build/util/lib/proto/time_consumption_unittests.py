#!/usr/bin/env vpython3

# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""File for testing time_consumption.py."""

import os
import tempfile
import unittest
import unittest.mock as mock

import google.protobuf.json_format as json_format

from google.protobuf import any_pb2

import measures

from test_script_metrics_pb2 import TestScriptMetric, TestScriptMetrics


class TimeConsumptionTest(unittest.TestCase):
  """Test time_consumption.py."""

  @mock.patch('time_consumption.time.time', side_effect=[100, 110])
  def test_by_dumping(self, time_patch) -> None:
    before = len(measures._metric._metrics)
    with measures.time_consumption('test', 'time', 'consumption'):
      pass
    with tempfile.TemporaryDirectory() as tmpdir:
      measures.dump(tmpdir)
      with open(os.path.join(tmpdir,
                             measures.TEST_SCRIPT_METRICS_JSONPB_FILENAME),
                'r',
                encoding='utf-8') as rf:
        any_msg = json_format.Parse(rf.read(), any_pb2.Any())
        message = TestScriptMetrics()
        self.assertTrue(any_msg.Unpack(message))
    self.assertEqual(len(message.metrics), before + 1)
    exp = TestScriptMetric()
    exp.name = 'test/time/consumption (seconds)'
    exp.value = 10
    self.assertEqual(message.metrics[-1], exp)
    self.assertEqual(time_patch.call_count, 2)

  @mock.patch('time_consumption.time.time', side_effect=[100, 101, 102, 110])
  def test_exit_twice(self, time_patch) -> None:
    # This is not a common use scenario, but it shouldn't crash.
    before = len(measures._metric._metrics)
    consumption = measures.time_consumption('test', 'time', 'consumption2')
    with consumption:
      pass
    with consumption:
      pass
    with tempfile.TemporaryDirectory() as tmpdir:
      measures.dump(tmpdir)
      with open(os.path.join(tmpdir,
                             measures.TEST_SCRIPT_METRICS_JSONPB_FILENAME),
                'r',
                encoding='utf-8') as rf:
        any_msg = json_format.Parse(rf.read(), any_pb2.Any())
        message = TestScriptMetrics()
        self.assertTrue(any_msg.Unpack(message))
    self.assertEqual(len(message.metrics), before + 1)
    exp = TestScriptMetric()
    exp.name = 'test/time/consumption2 (seconds)'
    exp.value = 8
    self.assertEqual(message.metrics[-1], exp)
    self.assertEqual(time_patch.call_count, 4)


if __name__ == '__main__':
  unittest.main()
