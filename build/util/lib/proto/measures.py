#!/usr/bin/env vpython3

# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
""" The module to create and manage measures using in the process. """

import json
import os

from google.protobuf import any_pb2
from google.protobuf.json_format import MessageToJson

from average import Average
from count import Count
from data_points import DataPoints
from measure import Measure
from metric import Metric
from time_consumption import TimeConsumption

# The file name is used as the key when being loaded into the ResultDB and
# shouldn't be changed.
TEST_SCRIPT_METRICS_JSONPB_FILENAME = 'test_script_metrics.jsonpb'

_metric = Metric()


def _create_name(*name_pieces: str) -> str:
  if len(name_pieces) == 0:
    raise ValueError('Need at least one name piece.')
  return '/'.join(list(name_pieces))


def _register(m: Measure) -> Measure:
  _metric.register(m)
  return m


def average(*name_pieces: str) -> Average:
  return _register(Average(_create_name(*name_pieces)))


def count(*name_pieces: str) -> Count:
  return _register(Count(_create_name(*name_pieces)))


def data_points(*name_pieces: str) -> DataPoints:
  return _register(DataPoints(_create_name(*name_pieces)))


def time_consumption(*name_pieces: str) -> TimeConsumption:
  return _register(TimeConsumption(_create_name(*name_pieces)))


# TODO(crbug.com/343242386): May need to implement a lock and reset logic to
# clear in-memory data and lock the instance to block further operations and
# avoid accidentally accumulating data which won't be published at all.
def dump(dir_path: str) -> None:
  """Dumps the metric data into test_script_metrics.jsonpb in the |path|."""
  os.makedirs(dir_path, exist_ok=True)
  any_msg = any_pb2.Any()
  any_msg.Pack(_metric.dump())
  with open(os.path.join(dir_path, TEST_SCRIPT_METRICS_JSONPB_FILENAME),
            'w',
            encoding='utf-8') as wf:
    wf.write(
        MessageToJson(any_msg, preserving_proto_field_name=True,
                      sort_keys=True))
