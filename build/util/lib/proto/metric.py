#!/usr/bin/env vpython3

# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
""" The entry of the metric system."""

from measure import Measure
from test_script_metrics_pb2 import TestScriptMetrics


class Metric:

  def __init__(self) -> None:
    self._metrics: List[Measure] = []

  def register(self, metric: Measure) -> None:
    self._metrics.append(metric)

  def size(self) -> int:
    return len(self._metrics)

  def clear(self) -> None:
    self._metrics.clear()

  def dump(self) -> TestScriptMetrics:
    result = TestScriptMetrics()
    result.metrics.extend([m.dump() for m in self._metrics])
    return result
