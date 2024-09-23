#!/usr/bin/env vpython3

# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
""" A metric implementation to record the raw inputs. """

from google.protobuf.timestamp_pb2 import Timestamp
from measure import Measure
from test_script_metrics_pb2 import TestScriptMetric


class DataPoints(Measure):

  def __init__(self, name: str) -> None:
    self._name = name
    self._points = []

  def record(self, value: float) -> None:
    point = TestScriptMetric.DataPoint()
    point.value = value
    # The function name is confusing, it updates itself to the current time.
    point.timestamp.GetCurrentTime()
    self._points.append(point)

  def dump(self) -> TestScriptMetric:
    result = TestScriptMetric()
    result.name = self._name
    result.points.points.extend(self._points)
    return result
