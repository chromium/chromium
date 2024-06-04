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

  # Dumping to the protobuf is mostly for testing purpose only. The real use
  # case would dump everything into a json file for the further upload.
  # TODO(crbug.com/343242386): May dump to a file once the file name is defined.
  def dump(self) -> TestScriptMetrics:
    result = TestScriptMetrics()
    result.metrics.extend([m.dump() for m in self._metrics])
    return result
