#!/usr/bin/env vpython3

# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""File for testing metric.py."""

import unittest
from average import Average
from count import Count
from metric import Metric


class MetricTest(unittest.TestCase):
  """Test metric.py."""

  def test_no_metric(self) -> None:
    m = Metric()
    self.assertFalse(m.dump().metrics)

  def test_multiple_metrics(self) -> None:
    m = Metric()
    m.register(Average("a"))
    m.register(Average("b"))
    m.register(Count("c"))
    self.assertEqual(len(m.dump().metrics), 3)
    self.assertEqual(set([i.name for i in m.dump().metrics]),
                     set(["a", "b", "c"]))

  def test_tags(self) -> None:
    m = Metric()
    m.register(Count("c"))
    m.tag("t1", "t2")
    m.tag("t3")
    self.assertEqual(len(m.dump().metrics), 4)
    self.assertEqual(set([i.name for i in m.dump().metrics]),
                     set(["c", "c@t1", "c@t2", "c@t3"]))



if __name__ == '__main__':
  unittest.main()
