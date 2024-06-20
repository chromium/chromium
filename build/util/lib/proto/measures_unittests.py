#!/usr/bin/env vpython3

# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""File for testing measures.py."""

import os
import tempfile
import unittest

import google.protobuf.json_format as json_format

from google.protobuf import any_pb2

import measures

from measure import Measure
from test_script_metrics_pb2 import TestScriptMetric, TestScriptMetrics


class MeasuresTest(unittest.TestCase):
  """Test measure.py."""

  def test_create_average(self) -> None:
    ave = measures.average('a', 'b', 'c')
    ave.record(1)
    ave.record(2)
    exp = TestScriptMetric()
    exp.name = 'a/b/c'
    exp.value = 1.5
    self.assertEqual(ave.dump(), exp)

  def test_create_single_name_piece(self) -> None:
    self.assertEqual(measures.average('a')._name, 'a')

  def test_create_no_name_piece(self) -> None:
    self.assertRaises(ValueError, lambda: measures.average())

  def test_create_none_in_name_pieces(self) -> None:
    self.assertRaises(TypeError, lambda: measures.average('a', None))

  def test_create_count(self) -> None:
    self.assertIsInstance(measures.count('a'), Measure)

  def test_create_data_points(self) -> None:
    self.assertIsInstance(measures.data_points('a'), Measure)

  def test_register(self) -> None:
    before = len(measures._metric._metrics)
    for x in range(3):
      measures.average('a')
    self.assertEqual(len(measures._metric._metrics), before + 3)

  def test_dump(self) -> None:
    before = len(measures._metric._metrics)
    count = measures.count('test', 'dump')
    for _ in range(101):
      count.record()
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
    exp.name = 'test/dump'
    exp.value = 101
    self.assertEqual(message.metrics[-1], exp)

  def test_dump_with_type(self) -> None:
    with tempfile.TemporaryDirectory() as tmpdir:
      measures.dump(tmpdir)
      with open(os.path.join(tmpdir,
                             measures.TEST_SCRIPT_METRICS_JSONPB_FILENAME),
                'r',
                encoding='utf-8') as rf:
        txt = rf.read()
        self.assertTrue('"@type":' in txt)
        self.assertTrue(TestScriptMetrics().DESCRIPTOR.full_name in txt)

  def test_dump_create_dir(self) -> None:
    with tempfile.TemporaryDirectory() as tmpdir:
      dir_path = os.path.join(tmpdir, 'hello', 'this', 'dir', 'should', 'not',
                              'exist')
      measures.dump(dir_path)
      self.assertTrue(os.path.isdir(dir_path))


if __name__ == '__main__':
  unittest.main()
