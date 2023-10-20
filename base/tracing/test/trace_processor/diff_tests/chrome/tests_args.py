#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from python.generators.diff_tests.testing import Path, DataPath, Metric
from python.generators.diff_tests.testing import Csv, Json, TextProto
from python.generators.diff_tests.testing import DiffTestBlueprint
from python.generators.diff_tests.testing import TestSuite


class ChromeArgs(TestSuite):
  # Unsymbolized args.
  def test_unsymbolized_args(self):
    return DiffTestBlueprint(
        trace=Path('unsymbolized_args.textproto'),
        query=Metric('chrome_unsymbolized_args'),
        out=TextProto(r"""
        [perfetto.protos.chrome_unsymbolized_args]: {
          args {
             module: "/liblib.so"
             build_id: "6275696c642d6964"
             address: 123
             google_lookup_id: "6275696c642d6964"
           }
           args {
             module: "/libmonochrome_64.so"
             build_id: "7f0715c286f8b16c10e4ad349cda3b9b56c7a773"
             address: 234
             google_lookup_id: "c215077ff8866cb110e4ad349cda3b9b0"
           }
        }
        """))

  def test_async_trace_1_count_slices(self):
    return DiffTestBlueprint(
        trace=DataPath('async-trace-1.json'),
        query="""
        SELECT COUNT(1) FROM slice;
        """,
        out=Csv("""
        "COUNT(1)"
        16
        """))

  def test_async_trace_2_count_slices(self):
    return DiffTestBlueprint(
        trace=DataPath('async-trace-2.json'),
        query="""
        SELECT COUNT(1) FROM slice;
        """,
        out=Csv("""
        "COUNT(1)"
        35
        """))

  # Chrome args class names
  def test_chrome_args_class_names(self):
    return DiffTestBlueprint(
        trace=TextProto(r"""
        packet {
          timestamp: 0
          incremental_state_cleared: true
          trusted_packet_sequence_id: 1
          track_descriptor {
            uuid: 12345
            thread {
              pid: 123
              tid: 345
            }
            parent_uuid: 0
            chrome_thread {
              thread_type: THREAD_POOL_FG_WORKER
            }
          }
        }

        packet {
          trusted_packet_sequence_id: 1
          timestamp: 0
          incremental_state_cleared: true
          track_event {
            track_uuid: 12345
            categories: "cat1"
            type: 3
            name: "name1"
            [perfetto.protos.ChromeTrackEvent.android_view_dump] {
              activity {
                name: "A"
                view {
                  class_name: "abc"
                },
                view {
                  class_name: "def"
                },
                view {
                  class_name: "ghi"
                }
              }
              activity {
                name: "B"
                view {
                  class_name: "jkl"
                }
              }
            }
          }
        }
        """),
        query=Metric('chrome_args_class_names'),
        out=TextProto(r"""

        [perfetto.protos.chrome_args_class_names] {
          class_names_per_version {
            class_name: "abc"
            class_name: "def"
            class_name: "ghi"
            class_name: "jkl"
          }
        }
        """))
