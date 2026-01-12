#!/usr/bin/env vpython3

# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
""" The entry of the metric system."""

from typing import List, Set

from measure import Measure
from test_script_metrics_pb2 import TestScriptMetrics


class Metric:

  def __init__(self) -> None:
    # A list of Measure to dump.
    self._metrics: List[Measure] = []
    # A list of tags to suffix the dumped results; see tag and dump function.
    self._tags: Set[str] = set()

  def register(self, metric: Measure) -> None:
    self._metrics.append(metric)

  def size(self) -> int:
    return len(self._metrics)

  def clear(self) -> None:
    self._metrics.clear()
    self._tags.clear()

  def tag(self, *args: str) -> None:
    # Tags the metrics, the tags will be merged and split by '|' and set to each
    # metric for easy selection.
    # This is an easy and hacky solution before adding output properties from
    # test script becomes possible. Currently adding output properties is
    # limited to the scope of the recipe, so any runtime tags are pretty much
    # impossible.
    self._tags.update(list(args))

  def dump(self) -> TestScriptMetrics:
    result = TestScriptMetrics()
    merged_tag = ''
    if self._tags:
      merged_tag = '|'
      for tag in sorted(self._tags):
        merged_tag += tag + '|'
    for m in self._metrics:
      metric = m.dump()
      metric.tag = merged_tag
      result.metrics.append(metric)
    return result
