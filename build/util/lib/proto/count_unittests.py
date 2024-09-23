#!/usr/bin/env vpython3

# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""File for testing count.py."""

import unittest
from count import Count


class CountTest(unittest.TestCase):
  """Test count.py."""

  def test_no_record(self) -> None:
    count = Count("a")
    self.assertEqual(count.dump().name, "a")
    self.assertEqual(count.dump().value, 0)

  def test_one_record(self) -> None:
    count = Count("b")
    count.record()
    self.assertEqual(count.dump().name, "b")
    self.assertEqual(count.dump().value, 1)

  def test_more_records(self) -> None:
    count = Count("c")
    count.record()
    count.record()
    self.assertEqual(count.dump().name, "c")
    self.assertEqual(count.dump().value, 2)


if __name__ == '__main__':
  unittest.main()
