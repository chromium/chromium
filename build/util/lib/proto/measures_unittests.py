#!/usr/bin/env vpython3

# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""File for testing measures.py."""

import measures
import unittest
from measure import Measure
from test_script_metrics_pb2 import TestScriptMetric


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


if __name__ == '__main__':
  unittest.main()
