#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from python.generators.diff_tests.testing import Path, DataPath, Metric
from python.generators.diff_tests.testing import Csv, Json, TextProto
from python.generators.diff_tests.testing import DiffTestBlueprint
from python.generators.diff_tests.testing import TestSuite


class ChromeMemorySnapshots(TestSuite):

  def test_memory_snapshot_general_validation(self):
    return DiffTestBlueprint(
        trace=DataPath('chrome_memory_snapshot.pftrace'),
        query="""
        SELECT
          (
            SELECT COUNT(*) FROM memory_snapshot
          ) AS total_snapshots,
          (
            SELECT COUNT(*) FROM process
          ) AS total_processes,
          (
            SELECT COUNT(*) FROM process_memory_snapshot
          ) AS total_process_snapshots,
          (
            SELECT COUNT(*) FROM memory_snapshot_node
          ) AS total_nodes,
          (
            SELECT COUNT(*) FROM memory_snapshot_edge
          ) AS total_edges,
          (
            SELECT COUNT(DISTINCT args.id)
            FROM args
            JOIN memory_snapshot_node
              ON args.arg_set_id = memory_snapshot_node.arg_set_id
          ) AS total_node_args,
          (
            SELECT COUNT(*) FROM profiler_smaps
            JOIN memory_snapshot ON timestamp = ts
          ) AS total_smaps;
        """,
        out=Path('memory_snapshot_general_validation.out'))

  def test_memory_snapshot_os_dump_events(self):
    return DiffTestBlueprint(
        trace=DataPath('chrome_memory_snapshot.pftrace'),
        query=Path('memory_snapshot_os_dump_events_test.sql'),
        out=Path('memory_snapshot_os_dump_events.out'))

  def test_memory_snapshot_chrome_dump_events(self):
    return DiffTestBlueprint(
        trace=DataPath('chrome_memory_snapshot.pftrace'),
        query="""
        SELECT
          pms.id AS process_snapshot_id,
          upid,
          snapshot_id,
          timestamp,
          detail_level
        FROM memory_snapshot ms
        LEFT JOIN process_memory_snapshot pms
          ON ms.id = pms.snapshot_id;
        """,
        out=Path('memory_snapshot_chrome_dump_events.out'))

  def test_memory_snapshot_nodes(self):
    return DiffTestBlueprint(
        trace=DataPath('chrome_memory_snapshot.pftrace'),
        query="""
        SELECT
          id,
          process_snapshot_id,
          parent_node_id,
          path,
          size,
          effective_size
        FROM memory_snapshot_node
        LIMIT 20;
        """,
        out=Path('memory_snapshot_nodes.out'))

  def test_memory_snapshot_edges(self):
    return DiffTestBlueprint(
        trace=DataPath('chrome_memory_snapshot.pftrace'),
        query="""
        SELECT
          id,
          source_node_id,
          target_node_id,
          importance
        FROM memory_snapshot_edge
        LIMIT 20;
        """,
        out=Path('memory_snapshot_edges.out'))

  def test_memory_snapshot_node_args(self):
    return DiffTestBlueprint(
        trace=DataPath('chrome_memory_snapshot.pftrace'),
        query="""
        SELECT
          node.id AS node_id,
          key,
          value_type,
          int_value,
          string_value
        FROM memory_snapshot_node node
        JOIN args ON node.arg_set_id = args.arg_set_id
        LIMIT 20;
        """,
        out=Path('memory_snapshot_node_args.out'))

  def test_memory_snapshot_smaps(self):
    return DiffTestBlueprint(
        trace=DataPath('chrome_memory_snapshot.pftrace'),
        query="""
        SELECT
          process.upid,
          process.name,
          smap.ts,
          path,
          size_kb,
          private_dirty_kb,
          swap_kb,
          file_name,
          start_address,
          module_timestamp,
          module_debugid,
          module_debug_path,
          protection_flags,
          private_clean_resident_kb,
          shared_dirty_resident_kb,
          shared_clean_resident_kb,
          locked_kb,
          proportional_resident_kb
        FROM process
        JOIN profiler_smaps smap ON process.upid = smap.upid
        JOIN memory_snapshot ms ON ms.timestamp = smap.ts
        LIMIT 20;
        """,
        out=Path('memory_snapshot_smaps.out'))
