#!/usr/bin/env vpython3

# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
""" A metric implementation to count the number of inputs. """

from measure import Measure
from test_script_metrics_pb2 import TestScriptMetric


class Count(Measure):

  def __init__(self, name: str) -> None:
    self._name = name
    self._count = 0

  def record(self) -> None:
    self._count += 1

  def dump(self) -> TestScriptMetric:
    result = TestScriptMetric()
    result.name = self._name
    result.value = self._count
    return result
