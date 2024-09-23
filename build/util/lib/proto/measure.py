#!/usr/bin/env vpython3

# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
""" The base class of all the measurement supported by the metric. """

from abc import ABC, abstractmethod
from test_script_metrics_pb2 import TestScriptMetric


class Measure(ABC):

  @abstractmethod
  def dump(self) -> TestScriptMetric:
    """Dumps the data into a TestScriptMetric instance.

    Returns:
        TestScriptMetric: A protobuf instance to represent the metric data.
    """
