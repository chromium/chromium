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

  # https://crrev.com/c/5634125 introduces new *ToPresentation slices,
  # and a new test trace file was added that contains them.
  # TODO(b/341047059): after M128 is rolled out to stable,
  # the test using the old trace can be removed.
  def test_chrome_frames_with_missed_vsyncs_m128(self):
    return DiffTestBlueprint(
        trace=DataPath('chrome_input_with_frame_view_new.pftrace'),
        query="""
        INCLUDE PERFETTO MODULE chrome.scroll_jank.scroll_jank_v3;

        SELECT
          cause_of_jank,
          sub_cause_of_jank,
          delay_since_last_frame,
          vsync_interval
        FROM chrome_janky_frames;
        """,
        out=Path('scroll_jank_v3_new.out'))

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
        trace=DataPath('chrome_input_with_frame_view.pftrace'),
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
        4328,1035865535981926,1255745000,1035865535981926,1035866753550926
        4471,1035866799527926,1358505000,1035866799527926,1035868108723926
        4620,1035868146266926,111786000,1035868146266926,1035868230937926
        4652,1035868607429926,1517121000,1035868607429926,1035870086449926
        """))

  def test_chrome_scroll_input_offsets(self):
    return DiffTestBlueprint(
        trace=DataPath('scroll_offsets_trace_2.pftrace'),
        query="""
        INCLUDE PERFETTO MODULE chrome.scroll_jank.scroll_offsets;

        SELECT
          scroll_update_id,
          ts,
          delta_y,
          relative_offset_y
        FROM chrome_scroll_input_offsets
        WHERE scroll_update_id IS NOT NULL
        ORDER by ts
        LIMIT 5;
        """,
        out=Csv("""
        "scroll_update_id","ts","delta_y","relative_offset_y"
        130,1349914859791,-6.932281,-6.932281
        132,1349923327791,-32.999954,-39.932235
        134,1349931893791,-39.999954,-79.932189
        136,1349940237791,-50.000076,-129.932266
        138,1349948670791,-57.999939,-187.932205
        """))

  def test_chrome_janky_event_latencies_v3(self):
    return DiffTestBlueprint(
        trace=DataPath('chrome_input_with_frame_view.pftrace'),
        query="""
        INCLUDE PERFETTO MODULE chrome.scroll_jank.scroll_jank_intervals;

        SELECT
          ts,
          dur,
          track_id,
          name,
          cause_of_jank,
          sub_cause_of_jank,
          delayed_frame_count,
          frame_jank_ts,
          frame_jank_dur
        FROM chrome_janky_event_latencies_v3;
        """,
        out=Csv("""
        "ts","dur","track_id","name","cause_of_jank","sub_cause_of_jank","delayed_frame_count","frame_jank_ts","frame_jank_dur"
        1035866897893926,49303000,968,"EventLatency","SubmitCompositorFrameToPresentationCompositorFrame","StartDrawToSwapStart",1,1035866935295926,11901000
        1035868162888926,61845000,1672,"EventLatency","SubmitCompositorFrameToPresentationCompositorFrame","StartDrawToSwapStart",1,1035868212811926,11921999
        1035868886494926,49285000,2055,"EventLatency","SubmitCompositorFrameToPresentationCompositorFrame","StartDrawToSwapStart",1,1035868923855926,11924000
        1035869208882926,60201000,2230,"EventLatency","SubmitCompositorFrameToPresentationCompositorFrame","BufferReadyToLatch",1,1035869257151926,11932000
        1035869319831926,71490000,2287,"EventLatency","SubmitCompositorFrameToPresentationCompositorFrame","StartDrawToSwapStart",1,1035869379377926,11944000
        1035869386651926,60311000,2314,"EventLatency","RendererCompositorQueueingDelay","[NULL]",1,1035869434949926,12013000
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
          delayed_frame_count
        FROM chrome_janky_frame_presentation_intervals
        ORDER by id;
        """,
        out=Csv("""
        "id","ts","dur","cause_of_jank","sub_cause_of_jank","delayed_frame_count"
        1,1035866935295926,11901000,"SubmitCompositorFrameToPresentationCompositorFrame","StartDrawToSwapStart",1
        2,1035868212811926,11921999,"SubmitCompositorFrameToPresentationCompositorFrame","StartDrawToSwapStart",1
        3,1035868923855926,11924000,"SubmitCompositorFrameToPresentationCompositorFrame","StartDrawToSwapStart",1
        4,1035869257151926,11932000,"SubmitCompositorFrameToPresentationCompositorFrame","BufferReadyToLatch",1
        5,1035869379377926,11944000,"SubmitCompositorFrameToPresentationCompositorFrame","StartDrawToSwapStart",1
        6,1035869434949926,12013000,"RendererCompositorQueueingDelay","[NULL]",1
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
        4328,"[NULL]",110,110,0,0.000000
        4471,1,120,119,1,0.840000
        4620,1,9,6,1,16.670000
        4652,4,133,129,4,3.100000
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
        1,1035866935295926,11901000
        2,1035868212811926,11921999
        3,1035868923855926,11924000
        4,1035869257151926,11932000
        5,1035869379377926,11944000
        6,1035869434949926,12013000
        """))
  def test_chrome_presented_scroll_offsets(self):
    return DiffTestBlueprint(
        trace=DataPath('scroll_offsets_trace_2.pftrace'),
        query="""
        INCLUDE PERFETTO MODULE chrome.scroll_jank.scroll_offsets;

        SELECT
          scroll_update_id,
          ts,
          delta_y,
          relative_offset_y
        FROM chrome_presented_scroll_offsets
        WHERE scroll_update_id IS NOT NULL
        ORDER by ts
        LIMIT 5;
        """,
        out=Csv("""
        "scroll_update_id","ts","delta_y","relative_offset_y"
        130,1349963342791,-6.932281,-6.932281
        132,1349985554791,-16.573090,-23.505371
        134,1349996680791,-107.517273,-131.022644
        140,1350007850791,-158.728424,-289.751068
        147,1350018935791,-89.808540,-379.559608
        """))

  def test_chrome_predictor_metrics(self):
    return DiffTestBlueprint(
        trace=DataPath('scroll_offsets_trace_2.pftrace'),
        query="""
        INCLUDE PERFETTO MODULE chrome.scroll_jank.predictor_error;

        SELECT
          scroll_update_id,
          present_ts,
          delta_y,
          prev_delta,
          next_delta,
          predictor_jank,
          delta_threshold
        FROM chrome_predictor_error
        WHERE scroll_update_id IS NOT NULL
        ORDER by present_ts
        LIMIT 5;
        """,
        out=Csv("""
        "scroll_update_id","present_ts","delta_y","prev_delta","next_delta","predictor_jank","delta_threshold"
        132,1349985554791,-16.573090,-6.932281,-107.517273,0.000000,1.200000
        134,1349996680791,-107.517273,-16.573090,-158.728424,0.000000,1.200000
        140,1350007850791,-158.728424,-107.517273,-89.808540,0.276306,1.200000
        147,1350018935791,-89.808540,-158.728424,-47.583618,0.000000,1.200000
        148,1350030066791,-47.583618,-89.808540,-98.283493,0.687384,1.200000
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

  def test_chrome_scroll_jank_with_pinches(self):
    return DiffTestBlueprint(
        trace=DataPath('scroll_jank_with_pinch.pftrace'),
        query="""
        INCLUDE PERFETTO MODULE chrome.scroll_jank.scroll_jank_v3;

        SELECT
          janks.cause_of_jank,
          janks.sub_cause_of_jank,
          janks.delay_since_last_frame,
          janks.event_latency_id,
          presented_frames.event_type
        FROM chrome_janky_frames janks
        LEFT JOIN chrome_presented_gesture_scrolls presented_frames
          ON janks.event_latency_id = presented_frames.id
        ORDER by event_latency_id;
        """,
        out=Csv("""
        "cause_of_jank","sub_cause_of_jank","delay_since_last_frame","event_latency_id","event_type"
        "SubmitCompositorFrameToPresentationCompositorFrame","StartDrawToSwapStart",22.252000,754,"GESTURE_SCROLL_UPDATE"
        "SubmitCompositorFrameToPresentationCompositorFrame","SubmitToReceiveCompositorFrame",22.263000,25683,"GESTURE_SCROLL_UPDATE"
        "SubmitCompositorFrameToPresentationCompositorFrame","ReceiveCompositorFrameToStartDraw",22.266000,26098,"GESTURE_SCROLL_UPDATE"
        "SubmitCompositorFrameToPresentationCompositorFrame","BufferReadyToLatch",22.262000,40846,"GESTURE_SCROLL_UPDATE"
        "BrowserMainToRendererCompositor","[NULL]",22.250000,50230,"GESTURE_SCROLL_UPDATE"
        "SubmitCompositorFrameToPresentationCompositorFrame","BufferReadyToLatch",22.267000,50517,"GESTURE_SCROLL_UPDATE"
        """))
