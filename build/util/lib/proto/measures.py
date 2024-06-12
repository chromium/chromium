#!/usr/bin/env vpython3

# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
""" The module to create and manage measures using in the process. """

from typing import Iterable

from average import Average
from count import Count
from data_points import DataPoints
from measure import Measure
from metric import Metric

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
