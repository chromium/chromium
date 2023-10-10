#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Processes the raw output from containers_memory_usage into CSV files. Each CSV
# file contains the results for all tested container types for a given key and
# value type.
#
# Usage:
# $ out/release/containers_memory_benchmark &> output.txt
# $ python3 analyze_containers_memory_benchmark.py < output.txt -o bench-results

import argparse
from collections.abc import Sequence
import csv
import os.path
import re
import sys
from typing import Optional


_HEADER_RE = re.compile(r'===== (?P<name>.+) =====')
_ITER_RE = re.compile(r'iteration (?P<iter>\d+)')
_ALLOC_RE = re.compile(r'alloc address (?P<alloc_addr>.+) size (?P<size>\d+)')
_FREED_RE = re.compile(r'freed address (?P<freed_addr>.+)')


class ContainerStatsProcessor:

  def __init__(self, name: str):
    # e.g. base::flat_map
    self._name = name
    # current number of elements in the container
    self._n = None
    # map of address to size for currently active allocations. Needed because
    # the free handler only records an address, and not a size.
    self._addr_to_size = {}
    # running count of the number of bytes needed at the current iteration
    self._running_size = 0
    # map of container size to number of bytes used to store a container of that
    # size. Keys are expected to be contiguous from 0 to the total iteration
    # count.
    self._data = {}

  @property
  def name(self):
    return self._name

  @property
  def data(self):
    return self._data

  def did_alloc(self, addr: str, size: int):
    self._addr_to_size[addr] = size
    self._running_size += size

  def did_free(self, addr: str):
    size = self._addr_to_size.pop(addr)
    self._running_size -= size

  def did_iterate(self, n: int):
    if self._n is not None:
      self.flush_current_iteration_if_needed()
    self._n = n

  def flush_current_iteration_if_needed(self):
    self._data[self._n] = self._running_size


class TestCaseProcessor:

  def __init__(self, name: str):
    # e.g. int -> std::string
    self._name = name
    # containers for which all allocation data has been processed and finalized.
    self._finalized_stats: list[ContainerStatsProcessor] = []
    # the current container being processed.
    self._current_container_stats: Optional[ContainerStatsProcessor] = None

  @property
  def current_container_stats(self):
    return self._current_container_stats

  def did_begin_container_stats(self, container_type: str):
    self._finalize_current_container_stats_if_needed()
    self._current_container_stats = ContainerStatsProcessor(container_type)

  def did_finish_container_stats(self, output_dir: str):
    self._finalize_current_container_stats_if_needed()
    with open(
        os.path.join(output_dir, f'{self._name}.csv'), 'w', newline=''
    ) as f:
      writer = csv.writer(f)
      # First the column headers...
      writer.writerow(
          ['size'] + [stats.name for stats in self._finalized_stats]
      )
      # In theory, all processed containers should have the same number of keys,
      # but assert just to be sure.
      keys = []
      for stats in self._finalized_stats:
        if not keys:
          keys = sorted(stats.data.keys())
        else:
          assert keys == sorted(stats.data.keys())
      for key in keys:
        writer.writerow(
            [key] + [stats.data[key] for stats in self._finalized_stats]
        )

  def _finalize_current_container_stats_if_needed(self):
    if self._current_container_stats:
      self._current_container_stats.flush_current_iteration_if_needed()
      self._finalized_stats.append(self._current_container_stats)
      self._current_container_stats = None


def main(argv: Sequence[str]) -> None:
  parser = argparse.ArgumentParser(
      description='Processes raw output from containers_memory_usage into CSVs.'
  )
  parser.add_argument(
      '-o', help='directory to write CSV files to', required=True
  )
  args = parser.parse_args()

  # It would be nicer to use a ContextManager, but that complicates splitting up
  # the input and iterating through it. This is "good enough".
  processor: Optional[TestCaseProcessor] = None

  for line in sys.stdin:
    line = line.strip()
    if '->' in line:
      if processor:
        processor.did_finish_container_stats(args.o)
      processor = TestCaseProcessor(line)
      continue

    match = _HEADER_RE.match(line)
    if match:
      processor.did_begin_container_stats(match.group('name'))

    match = _ITER_RE.match(line)
    if match:
      processor.current_container_stats.did_iterate(int(match.group('iter')))
      continue

    match = _ALLOC_RE.match(line)
    if match:
      processor.current_container_stats.did_alloc(
          match.group('alloc_addr'), int(match.group('size'))
      )
      continue

    match = _FREED_RE.match(line)
    if match:
      processor.current_container_stats.did_free(match.group('freed_addr'))
      continue

  if processor:
    processor.did_finish_container_stats(args.o)


if __name__ == '__main__':
  main(sys.argv)
