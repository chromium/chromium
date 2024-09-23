#!/usr/bin/env vpython3

# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
""" A metric implementation to calculate the average of the inputs. """

from measure import Measure
from test_script_metrics_pb2 import TestScriptMetric


class Average(Measure):

  def __init__(self, name: str) -> None:
    self._name = name
    self._value = 0
    self._count = 0

  def record(self, value: float) -> None:
    self._value = (self._value * self._count + value) / (self._count + 1)
    self._count += 1

  def dump(self) -> TestScriptMetric:
    result = TestScriptMetric()
    result.name = self._name
    result.value = self._value
    return result
