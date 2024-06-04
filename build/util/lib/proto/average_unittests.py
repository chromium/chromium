#!/usr/bin/env vpython3

# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""File for testing average.py."""

import unittest
from average import Average


class AverageTest(unittest.TestCase):
  """Test average.py."""

  def test_no_record(self) -> None:
    ave = Average("a")
    self.assertEqual(ave.dump().name, "a")
    self.assertEqual(ave.dump().value, 0)

  def test_one_record(self) -> None:
    ave = Average("b")
    ave.record(101)
    self.assertEqual(ave.dump().name, "b")
    self.assertEqual(ave.dump().value, 101)

  def test_more_records(self) -> None:
    ave = Average("c")
    ave.record(1)
    ave.record(2)
    ave.record(3)
    ave.record(4)
    self.assertEqual(ave.dump().name, "c")
    self.assertEqual(ave.dump().value, 2.5)


if __name__ == '__main__':
  unittest.main()
