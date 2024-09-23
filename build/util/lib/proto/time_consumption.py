#!/usr/bin/env vpython3

# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
""" A metric implementation to calculate the time consumption.

Example:
  with TimeConsumption('foo'):
    do_something()
"""

import time

from contextlib import AbstractContextManager

from measure import Measure
from test_script_metrics_pb2 import TestScriptMetric


class TimeConsumption(AbstractContextManager, Measure):

  def __init__(self, name: str) -> None:
    self._name = name + ' (seconds)'
    self._start = 0
    self._end = 0

  def __enter__(self) -> None:
    self._start = time.time()

  def __exit__(self, exc_type, exc_value, traceback) -> bool:
    self._end = time.time()
    # Do not suppress exceptions.
    return False

  def dump(self) -> TestScriptMetric:
    result = TestScriptMetric()
    result.name = self._name
    result.value = (self._end - self._start)
    return result
