#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from python.generators.diff_tests.testing import Path, DataPath, Metric
from python.generators.diff_tests.testing import Csv, Json, TextProto
from python.generators.diff_tests.testing import DiffTestBlueprint
from python.generators.diff_tests.testing import TestSuite


class ChromeScrollJankStdlib(TestSuite):

  def test_chrome_frames_with_missed_vsyncs(self):
    return DiffTestBlueprint(
        trace=DataPath('chrome_input_with_frame_view.pftrace'),
        query="""
        INCLUDE PERFETTO MODULE chrome.scroll_jank.scroll_jank_v3;

        SELECT
          cause_of_jank,
          sub_cause_of_jank,
          delay_since_last_frame,
          vsync_interval
        FROM chrome_janky_frames;
        """,
        out=Path('scroll_jank_v3.out'))

  def test_chrome_frames_with_missed_vsyncs_percentage(self):
    return DiffTestBlueprint(
        trace=DataPath('chrome_input_with_frame_view.pftrace'),
        query="""
        INCLUDE PERFETTO MODULE chrome.scroll_jank.scroll_jank_v3;

        SELECT
          delayed_frame_percentage
        FROM chrome_janky_frames_percentage;
        """,
        out=Path('scroll_jank_v3_percentage.out'))

  def test_chrome_scrolls(self):
    return DiffTestBlueprint(
        trace=Path('chrome_scroll_check.py'),
        query="""
        INCLUDE PERFETTO MODULE chrome.chrome_scrolls;

        SELECT
          id,
          ts,
          dur,
          gesture_scroll_begin_ts,
          gesture_scroll_end_ts
        FROM chrome_scrolls
        ORDER by id;
        """,
        out=Csv("""
        "id","ts","dur","gesture_scroll_begin_ts","gesture_scroll_end_ts"
        5678,0,55000000,0,45000000
        5679,60000000,40000000,60000000,90000000
        5680,80000000,30000000,80000000,100000000
        5681,120000000,70000000,120000000,"[NULL]"
        """))

  def test_chrome_scroll_intervals(self):
    return DiffTestBlueprint(
        trace=Path('chrome_scroll_check.py'),
        query="""
        INCLUDE PERFETTO MODULE chrome.chrome_scrolls;

        SELECT
          id,
          ts,
          dur
        FROM chrome_scrolling_intervals
        ORDER by id;
        """,
        out=Csv("""
        "id","ts","dur"
        1,0,55000000
        2,60000000,50000000
        3,120000000,70000000
        """))

  def test_chrome_scroll_input_offsets(self):
    return DiffTestBlueprint(
        trace=DataPath('scroll_offsets.pftrace'),
        query="""
        INCLUDE PERFETTO MODULE chrome.scroll_jank.scroll_offsets;

        SELECT
          scroll_update_id,
          ts,
          delta_y,
          offset_y
        FROM chrome_scroll_input_offsets
        ORDER by ts
        LIMIT 5;
        """,
        out=Csv("""
        "scroll_update_id","ts","delta_y","offset_y"
        1983,4687296612739,-36.999939,-36.999939
        1983,4687307175845,-39.000092,-76.000031
        1987,4687313206739,-35.999969,-112.000000
        1987,4687323152462,-35.000000,-147.000000
        1991,4687329240739,-28.999969,-175.999969
        """))

  def test_chrome_janky_event_latencies_v3(self):
    return DiffTestBlueprint(
        trace=DataPath('chrome_input_with_frame_view.pftrace'),
        query="""
        INCLUDE PERFETTO MODULE chrome.scroll_jank.scroll_jank_intervals;

        SELECT
          id,
          ts,
          dur,
          track_id,
          name,
          cause_of_jank,
          sub_cause_of_jank,
          delayed_frame_count,
          frame_jank_ts,
          frame_jank_dur
        FROM chrome_janky_event_latencies_v3
        ORDER by id;
        """,
        out=Csv("""
        "id","ts","dur","track_id","name","cause_of_jank","sub_cause_of_jank","delayed_frame_count","frame_jank_ts","frame_jank_dur"
        29926,174795897267797,48088000,1431,"EventLatency","RendererCompositorQueueingDelay","[NULL]",1,174795928261797,17094000
        38463,174796315541797,131289000,2163,"EventLatency","RendererCompositorFinishedToBeginImplFrame","[NULL]",5,174796362924797,83906000
        88876,174799556245797,49856000,4329,"EventLatency","RendererCompositorQueueingDelay","[NULL]",1,174799589065797,17036000
        """))

  def test_chrome_janky_frame_presentation_intervals(self):
    return DiffTestBlueprint(
        trace=DataPath('chrome_input_with_frame_view.pftrace'),
        query="""
        INCLUDE PERFETTO MODULE chrome.scroll_jank.scroll_jank_intervals;

        SELECT
          id,
          ts,
          dur,
          cause_of_jank,
          sub_cause_of_jank,
          delayed_frame_count,
          event_latency_id
        FROM chrome_janky_frame_presentation_intervals
        ORDER by id;
        """,
        out=Csv("""
        "id","ts","dur","cause_of_jank","sub_cause_of_jank","delayed_frame_count","event_latency_id"
        1,174795928261797,17094000,"RendererCompositorQueueingDelay","[NULL]",1,29926
        2,174796362924797,83906000,"RendererCompositorFinishedToBeginImplFrame","[NULL]",5,38463
        3,174799589065797,17036000,"RendererCompositorQueueingDelay","[NULL]",1,88876
        """))

  def test_chrome_scroll_stats(self):
    return DiffTestBlueprint(
        trace=DataPath('chrome_input_with_frame_view.pftrace'),
        query="""
        INCLUDE PERFETTO MODULE chrome.scroll_jank.scroll_jank_intervals;

        SELECT
          scroll_id,
          missed_vsyncs,
          frame_count,
          presented_frame_count,
          janky_frame_count,
          janky_frame_percent
        FROM chrome_scroll_stats
        ORDER by scroll_id;
        """,
        out=Csv("""
        "scroll_id","missed_vsyncs","frame_count","presented_frame_count","janky_frame_count","janky_frame_percent"
        1186,6,110,105,2,1.900000
        1889,"[NULL]",101,102,0,0.000000
        2506,1,84,84,1,1.190000
        """))

  def test_chrome_scroll_jank_intervals_v3(self):
    return DiffTestBlueprint(
        trace=DataPath('chrome_input_with_frame_view.pftrace'),
        query="""
        INCLUDE PERFETTO MODULE chrome.scroll_jank.scroll_jank_intervals;

        SELECT
          id,
          ts,
          dur
        FROM chrome_scroll_jank_intervals_v3
        ORDER by id;
        """,
        out=Csv("""
        "id","ts","dur"
        1,174795928261797,17094000
        2,174796362924797,83906000
        3,174799589065797,17036000
        """))
  def test_chrome_presented_scroll_offsets(self):
    return DiffTestBlueprint(
        trace=DataPath('scroll_offsets.pftrace'),
        query="""
        INCLUDE PERFETTO MODULE chrome.scroll_jank.scroll_offsets;

        SELECT
          scroll_update_id,
          ts,
          delta_y,
          offset_y
        FROM chrome_presented_scroll_offsets
        ORDER by ts
        LIMIT 5;
        """,
        out=Csv("""
        "scroll_update_id","ts","delta_y","offset_y"
        1983,4687341817739,"[NULL]",0
        1987,4687352950739,-50,-50
        1991,4687364083739,-50,-100
        1993,4687375224739,-81,-181
        1996,4687386343739,-66,-247
        """))

  def test_scroll_jank_cause_map(self):
    return DiffTestBlueprint(
        trace=TextProto(''),
        query="""
        INCLUDE PERFETTO MODULE chrome.event_latency_description;
        INCLUDE PERFETTO MODULE chrome.scroll_jank.scroll_jank_cause_map;

        SELECT
          DISTINCT event_latency_stage
        FROM chrome_scroll_jank_cause_descriptions
        WHERE event_latency_stage NOT IN
          (
            SELECT
              DISTINCT name
            FROM chrome_event_latency_stage_descriptions
          );
        """,
        # Empty output is expected to ensure that all scroll jank causes
        # correspond to a valid EventLatency stage.
        out=Csv("""
        "event_latency_stage"
        """))
