#!/usr/bin/env vpython3

# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""File for testing data_points.py."""

import unittest
from data_points import DataPoints


class DataPointsTest(unittest.TestCase):
  """Test data_points.py."""

  def test_no_record(self) -> None:
    points = DataPoints("a")
    self.assertEqual(points.dump().name, "a")
    self.assertEqual(len(points.dump().points.points), 0)

  def test_one_record(self) -> None:
    points = DataPoints("b")
    points.record(101)
    self.assertEqual(points.dump().name, "b")
    self.assertEqual(len(points.dump().points.points), 1)
    self.assertEqual(points.dump().points.points[0].value, 101)

  def test_more_records(self) -> None:
    points = DataPoints("c")
    points.record(1)
    points.record(2)
    self.assertEqual(points.dump().name, "c")
    self.assertEqual(len(points.dump().points.points), 2)
    self.assertEqual(points.dump().points.points[0].value, 1)
    self.assertEqual(points.dump().points.points[1].value, 2)


if __name__ == '__main__':
  unittest.main()
