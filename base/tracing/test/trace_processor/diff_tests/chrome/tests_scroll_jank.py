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

  def test_chrome_event_latencies(self):
    return DiffTestBlueprint(
        trace=DataPath('chrome_input_with_frame_view_new.pftrace'),
        query="""
        INCLUDE PERFETTO MODULE chrome.event_latency;

        SELECT
          id,
          name,
          ts,
          dur,
          scroll_update_id,
          is_presented,
          event_type,
          track_id,
          vsync_interval_ms,
          is_janky_scrolled_frame,
          buffer_available_timestamp,
          buffer_ready_timestamp,
          latch_timestamp,
          swap_end_timestamp,
          presentation_timestamp
        FROM chrome_event_latencies
        WHERE
          (
            event_type = 'GESTURE_SCROLL_UPDATE'
            OR event_type = 'INERTIAL_GESTURE_SCROLL_UPDATE')
          AND is_presented
        ORDER BY id
        LIMIT 10;
        """,
        out=Csv("""
        "id","name","ts","dur","scroll_update_id","is_presented","event_type","track_id","vsync_interval_ms","is_janky_scrolled_frame","buffer_available_timestamp","buffer_ready_timestamp","latch_timestamp","swap_end_timestamp","presentation_timestamp"
        69,"EventLatency",4488833086777189,49497000,10,1,"GESTURE_SCROLL_UPDATE",14,11.111000,0,4488833114547189,4488833114874189,4488833119872189,4488833126765189,4488833136274189
        431,"EventLatency",4488833114107189,33292000,14,1,"INERTIAL_GESTURE_SCROLL_UPDATE",27,11.111000,0,"[NULL]",4488833122361189,4488833137159189,4488833138924189,4488833147399189
        480,"EventLatency",4488833125213189,33267000,15,1,"INERTIAL_GESTURE_SCROLL_UPDATE",29,11.111000,0,4488833131905189,4488833132275189,4488833148524189,4488833150809189,4488833158480189
        531,"EventLatency",4488833142387189,27234000,16,1,"INERTIAL_GESTURE_SCROLL_UPDATE",32,11.111000,0,4488833147322189,4488833147657189,4488833159447189,4488833161654189,4488833169621189
        581,"EventLatency",4488833153584189,27225000,17,1,"INERTIAL_GESTURE_SCROLL_UPDATE",34,11.111000,0,4488833158111189,4488833158333189,4488833170433189,4488833171562189,4488833180809189
        630,"EventLatency",4488833164707189,27227000,18,1,"INERTIAL_GESTURE_SCROLL_UPDATE",36,11.111000,0,4488833169215189,4488833169529189,4488833181700189,4488833183880189,4488833191934189
        679,"EventLatency",4488833175814189,27113000,19,1,"INERTIAL_GESTURE_SCROLL_UPDATE",38,11.111000,0,4488833180589189,4488833180876189,4488833192722189,4488833194160189,4488833202927189
        728,"EventLatency",4488833186929189,38217000,20,1,"INERTIAL_GESTURE_SCROLL_UPDATE",40,11.111000,1,4488833201398189,4488833201459189,4488833215357189,4488833217727189,4488833225146189
        772,"EventLatency",4488833198068189,38185000,21,1,"INERTIAL_GESTURE_SCROLL_UPDATE",42,11.111000,0,4488833211744189,4488833212097189,4488833226028189,4488833227246189,4488833236253189
        819,"EventLatency",4488833209202189,38170000,22,1,"INERTIAL_GESTURE_SCROLL_UPDATE",43,11.111000,0,4488833223115189,4488833223308189,4488833237115189,4488833238196189,4488833247372189
        """))

  # A trace from M131 (ToT as of adding this test) has the necessary
  # events/arguments.
  def test_chrome_input_pipeline_steps(self):
        return DiffTestBlueprint(
        trace=DataPath('scroll_m131.pftrace'),
        query="""
        INCLUDE PERFETTO MODULE chrome.input;

        SELECT latency_id,
          input_type,
          GROUP_CONCAT(step) AS steps
        FROM chrome_input_pipeline_steps
        GROUP BY latency_id
        ORDER by input_type
        LIMIT 20
        """,
        out=Csv("""
        "latency_id","input_type","steps"
        -2143831735395279846,"GESTURE_FLING_CANCEL_EVENT","STEP_SEND_INPUT_EVENT_UI"
        -2143831735395279570,"GESTURE_FLING_CANCEL_EVENT","STEP_SEND_INPUT_EVENT_UI"
        -2143831735395279037,"GESTURE_FLING_CANCEL_EVENT","STEP_SEND_INPUT_EVENT_UI"
        -2143831735395280234,"GESTURE_FLING_START_EVENT","STEP_SEND_INPUT_EVENT_UI"
        -2143831735395279756,"GESTURE_FLING_START_EVENT","STEP_SEND_INPUT_EVENT_UI"
        -2143831735395279516,"GESTURE_FLING_START_EVENT","STEP_SEND_INPUT_EVENT_UI"
        -2143831735395278975,"GESTURE_FLING_START_EVENT","STEP_SEND_INPUT_EVENT_UI"
        -2143831735395280167,"GESTURE_SCROLL_BEGIN_EVENT","STEP_SEND_INPUT_EVENT_UI,STEP_HANDLE_INPUT_EVENT_IMPL,STEP_DID_HANDLE_INPUT_AND_OVERSCROLL,STEP_GESTURE_EVENT_HANDLED"
        -2143831735395279816,"GESTURE_SCROLL_BEGIN_EVENT","STEP_SEND_INPUT_EVENT_UI,STEP_HANDLE_INPUT_EVENT_IMPL,STEP_DID_HANDLE_INPUT_AND_OVERSCROLL,STEP_GESTURE_EVENT_HANDLED"
        -2143831735395279175,"GESTURE_SCROLL_BEGIN_EVENT","STEP_SEND_INPUT_EVENT_UI,STEP_HANDLE_INPUT_EVENT_IMPL,STEP_DID_HANDLE_INPUT_AND_OVERSCROLL,STEP_GESTURE_EVENT_HANDLED"
        -2143831735395279004,"GESTURE_SCROLL_BEGIN_EVENT","STEP_SEND_INPUT_EVENT_UI,STEP_HANDLE_INPUT_EVENT_IMPL,STEP_DID_HANDLE_INPUT_AND_OVERSCROLL,STEP_GESTURE_EVENT_HANDLED"
        -2143831735395280198,"GESTURE_SCROLL_END_EVENT","STEP_SEND_INPUT_EVENT_UI,STEP_GESTURE_EVENT_HANDLED,STEP_HANDLE_INPUT_EVENT_IMPL,STEP_DID_HANDLE_INPUT_AND_OVERSCROLL"
        -2143831735395279762,"GESTURE_SCROLL_END_EVENT","STEP_SEND_INPUT_EVENT_UI,STEP_GESTURE_EVENT_HANDLED,STEP_HANDLE_INPUT_EVENT_IMPL,STEP_DID_HANDLE_INPUT_AND_OVERSCROLL"
        -2143831735395279584,"GESTURE_SCROLL_END_EVENT","STEP_SEND_INPUT_EVENT_UI,STEP_GESTURE_EVENT_HANDLED,STEP_HANDLE_INPUT_EVENT_IMPL,STEP_DID_HANDLE_INPUT_AND_OVERSCROLL"
        -2143831735395279038,"GESTURE_SCROLL_END_EVENT","STEP_SEND_INPUT_EVENT_UI,STEP_GESTURE_EVENT_HANDLED,STEP_HANDLE_INPUT_EVENT_IMPL,STEP_DID_HANDLE_INPUT_AND_OVERSCROLL"
        -2143831735395280256,"GESTURE_SCROLL_UPDATE_EVENT","STEP_SEND_INPUT_EVENT_UI,STEP_HANDLE_INPUT_EVENT_IMPL,STEP_DID_HANDLE_INPUT_AND_OVERSCROLL,STEP_GESTURE_EVENT_HANDLED"
        -2143831735395280254,"GESTURE_SCROLL_UPDATE_EVENT","STEP_SEND_INPUT_EVENT_UI,STEP_HANDLE_INPUT_EVENT_IMPL,STEP_DID_HANDLE_INPUT_AND_OVERSCROLL,STEP_GESTURE_EVENT_HANDLED"
        -2143831735395280250,"GESTURE_SCROLL_UPDATE_EVENT","STEP_SEND_INPUT_EVENT_UI,STEP_HANDLE_INPUT_EVENT_IMPL,STEP_DID_HANDLE_INPUT_AND_OVERSCROLL,STEP_GESTURE_EVENT_HANDLED"
        -2143831735395280248,"GESTURE_SCROLL_UPDATE_EVENT","STEP_SEND_INPUT_EVENT_UI,STEP_HANDLE_INPUT_EVENT_IMPL,STEP_DID_HANDLE_INPUT_AND_OVERSCROLL,STEP_GESTURE_EVENT_HANDLED"
        -2143831735395280246,"GESTURE_SCROLL_UPDATE_EVENT","STEP_SEND_INPUT_EVENT_UI,STEP_HANDLE_INPUT_EVENT_IMPL,STEP_DID_HANDLE_INPUT_AND_OVERSCROLL,STEP_GESTURE_EVENT_HANDLED"
        """))

  def test_task_start_time_input_pipeline(self):
    return DiffTestBlueprint(
        trace=DataPath('scroll_m131.pftrace'),
        query="""
        INCLUDE PERFETTO MODULE chrome.input;

        SELECT
          latency_id,
          step,
          task_start_time_ts
        FROM chrome_input_pipeline_steps
        ORDER BY latency_id
        LIMIT 10;
        """,
        # STEP_SEND_INPUT_EVENT_UI does not run in a task,
        # so its task_start_time_ts will be NULL.
        out=Csv("""
        "latency_id","step","task_start_time_ts"
        -2143831735395280256,"STEP_SEND_INPUT_EVENT_UI","[NULL]"
        -2143831735395280256,"STEP_HANDLE_INPUT_EVENT_IMPL",1292554143003210
        -2143831735395280256,"STEP_DID_HANDLE_INPUT_AND_OVERSCROLL",1292554153539210
        -2143831735395280256,"STEP_GESTURE_EVENT_HANDLED",1292554154651257
        -2143831735395280254,"STEP_SEND_INPUT_EVENT_UI","[NULL]"
        -2143831735395280254,"STEP_HANDLE_INPUT_EVENT_IMPL",1292554155188210
        -2143831735395280254,"STEP_DID_HANDLE_INPUT_AND_OVERSCROLL",1292554164359210
        -2143831735395280254,"STEP_GESTURE_EVENT_HANDLED",1292554165141257
        -2143831735395280250,"STEP_SEND_INPUT_EVENT_UI","[NULL]"
        -2143831735395280250,"STEP_HANDLE_INPUT_EVENT_IMPL",1292554131865210
        """))

  def test_task_start_time_surface_frame_steps(self):
    return DiffTestBlueprint(
      trace=DataPath('scroll_m132.pftrace'),
      query="""
      INCLUDE PERFETTO MODULE chrome.graphics_pipeline;
      SELECT
        step,
        task_start_time_ts
      FROM chrome_graphics_pipeline_surface_frame_steps
      ORDER BY ts
      LIMIT 10;
      """,
      out=Csv("""
      "step","task_start_time_ts"
      "STEP_ISSUE_BEGIN_FRAME","[NULL]"
      "STEP_RECEIVE_BEGIN_FRAME",3030298007485995
      "STEP_GENERATE_COMPOSITOR_FRAME",3030298014657995
      "STEP_SUBMIT_COMPOSITOR_FRAME",3030298014658995
      "STEP_RECEIVE_COMPOSITOR_FRAME",3030298015629268
      "STEP_ISSUE_BEGIN_FRAME",3030298016857268
      "STEP_GENERATE_COMPOSITOR_FRAME",3030298017600363
      "STEP_ISSUE_BEGIN_FRAME","[NULL]"
      "STEP_ISSUE_BEGIN_FRAME","[NULL]"
      "STEP_SUBMIT_COMPOSITOR_FRAME",3030298017634363
      """))

  def test_task_start_time_display_frame_steps(self):
    return DiffTestBlueprint(
      trace=DataPath('scroll_m132.pftrace'),
      query="""
      INCLUDE PERFETTO MODULE chrome.graphics_pipeline;
      SELECT
        step,
        task_start_time_ts
      FROM chrome_graphics_pipeline_display_frame_steps
      ORDER BY ts
      LIMIT 10;
      """,
      out=Csv("""
      "step","task_start_time_ts"
      "STEP_DRAW_AND_SWAP",3030298019565268
      "STEP_SURFACE_AGGREGATION",3030298019563268
      "STEP_SEND_BUFFER_SWAP",3030298019563268
      "STEP_BUFFER_SWAP_POST_SUBMIT",3030298020965472
      "STEP_DRAW_AND_SWAP",3030298029758268
      "STEP_SURFACE_AGGREGATION",3030298029755268
      "STEP_SEND_BUFFER_SWAP",3030298029755268
      "STEP_BUFFER_SWAP_POST_SUBMIT",3030298031460472
      "STEP_DRAW_AND_SWAP",3030298041020268
      "STEP_SURFACE_AGGREGATION",3030298041017268
      """))

  def test_chrome_coalesced_inputs(self):
        return DiffTestBlueprint(
        trace=DataPath('scroll_m131.pftrace'),
        query="""
        INCLUDE PERFETTO MODULE chrome.input;

        SELECT
          coalesced_latency_id,
          presented_latency_id
        FROM chrome_coalesced_inputs
        ORDER BY coalesced_latency_id
        LIMIT 10
        """,
        out=Csv("""
        "coalesced_latency_id","presented_latency_id"
        -2143831735395280239,-2143831735395280239
        -2143831735395280183,-2143831735395280179
        -2143831735395280179,-2143831735395280179
        -2143831735395280166,-2143831735395280166
        -2143831735395280158,-2143831735395280153
        -2143831735395280153,-2143831735395280153
        -2143831735395280150,-2143831735395280146
        -2143831735395280146,-2143831735395280146
        -2143831735395280144,-2143831735395280139
        -2143831735395280139,-2143831735395280139
        """))

  def test_chrome_touch_move_to_scroll_update(self):
        return DiffTestBlueprint(
        trace=DataPath('scroll_m131.pftrace'),
        query="""
        INCLUDE PERFETTO MODULE chrome.input;

        SELECT
          touch_move_latency_id,
          scroll_update_latency_id
        FROM chrome_touch_move_to_scroll_update
        ORDER BY touch_move_latency_id
        LIMIT 10
        """,
        out=Csv("""
        "touch_move_latency_id","scroll_update_latency_id"
        -2143831735395280236,-2143831735395280239
        -2143831735395280189,-2143831735395280179
        -2143831735395280181,-2143831735395280139
        -2143831735395280177,-2143831735395280183
        -2143831735395280163,-2143831735395280166
        -2143831735395280160,-2143831735395280158
        -2143831735395280155,-2143831735395280153
        -2143831735395280152,-2143831735395280150
        -2143831735395280148,-2143831735395280146
        -2143831735395280142,-2143831735395280132
        """))

  def test_chrome_scroll_update_info(self):
        return DiffTestBlueprint(
        trace=DataPath('scroll_m131.pftrace'),
        query="""
        INCLUDE PERFETTO MODULE chrome.chrome_scrolls;

        SELECT
          id,
          vsync_interval_ms,
          is_presented,
          is_janky,
          is_inertial,
          is_first,
          is_coalesced,
          generation_ts,
          touch_move_received_ts,
          generation_to_browser_main_dur,
          scroll_update_created_ts,
          scroll_update_created_end_ts,
          touch_move_processing_dur,
          scroll_update_processing_dur,
          compositor_dispatch_ts,
          compositor_dispatch_end_ts,
          browser_to_compositor_delay_dur,
          compositor_dispatch_dur,
          compositor_on_begin_frame_ts,
          compositor_on_begin_frame_end_ts,
          compositor_dispatch_to_on_begin_frame_delay_dur,
          compositor_on_begin_frame_dur,
          compositor_generate_compositor_frame_ts,
          compositor_on_begin_frame_to_generation_delay_dur,
          compositor_submit_compositor_frame_ts,
          compositor_submit_compositor_frame_end_ts,
          compositor_generate_frame_to_submit_frame_dur,
          compositor_submit_frame_dur,
          viz_receive_compositor_frame_ts,
          viz_receive_compositor_frame_end_ts,
          compositor_to_viz_delay_dur,
          viz_receive_compositor_frame_dur,
          viz_draw_and_swap_ts,
          viz_wait_for_draw_dur,
          viz_send_buffer_swap_end_ts,
          viz_draw_and_swap_dur,
          viz_swap_buffers_ts,
          viz_swap_buffers_end_ts,
          viz_to_gpu_delay_dur,
          viz_swap_buffers_dur,
          latch_timestamp,
          swap_end_timestamp,
          presentation_timestamp,
          viz_swap_buffers_to_latch_dur,
          viz_latch_to_swap_end_dur,
          swap_end_to_presentation_dur
        FROM chrome_scroll_update_info
        ORDER BY id
        LIMIT 21
        """,
        out=Csv("""
        "id","vsync_interval_ms","is_presented","is_janky","is_inertial","is_first","is_coalesced","generation_ts","touch_move_received_ts","generation_to_browser_main_dur","scroll_update_created_ts","scroll_update_created_end_ts","touch_move_processing_dur","scroll_update_processing_dur","compositor_dispatch_ts","compositor_dispatch_end_ts","browser_to_compositor_delay_dur","compositor_dispatch_dur","compositor_on_begin_frame_ts","compositor_on_begin_frame_end_ts","compositor_dispatch_to_on_begin_frame_delay_dur","compositor_on_begin_frame_dur","compositor_generate_compositor_frame_ts","compositor_on_begin_frame_to_generation_delay_dur","compositor_submit_compositor_frame_ts","compositor_submit_compositor_frame_end_ts","compositor_generate_frame_to_submit_frame_dur","compositor_submit_frame_dur","viz_receive_compositor_frame_ts","viz_receive_compositor_frame_end_ts","compositor_to_viz_delay_dur","viz_receive_compositor_frame_dur","viz_draw_and_swap_ts","viz_wait_for_draw_dur","viz_send_buffer_swap_end_ts","viz_draw_and_swap_dur","viz_swap_buffers_ts","viz_swap_buffers_end_ts","viz_to_gpu_delay_dur","viz_swap_buffers_dur","latch_timestamp","swap_end_timestamp","presentation_timestamp","viz_swap_buffers_to_latch_dur","viz_latch_to_swap_end_dur","swap_end_to_presentation_dur"
        -2143831735395280256,11.111000,1,0,1,0,0,1292554141489270,"[NULL]","[NULL]",1292554142167257,1292554142530257,"[NULL]",363000,1292554143003210,1292554143111210,472953,108000,1292554154023210,1292554154106210,10912000,83000,1292554154282210,176000,1292554154619210,1292554154956210,337000,337000,1292554155095633,1292554155221633,139423,126000,1292554155286633,65000,1292554156581633,1295000,1292554158202131,1292554158738131,1620498,536000,1292554169636270,1292554176454270,1292554185799270,10898139,6818000,9345000
        -2143831735395280254,11.111000,1,0,1,0,0,1292554152575270,"[NULL]","[NULL]",1292554154230257,1292554154489257,"[NULL]",259000,1292554155188210,1292554155308210,698953,120000,1292554164945210,1292554165168210,9637000,223000,1292554165562210,394000,1292554165949210,1292554166312210,387000,363000,1292554166460633,1292554166589633,148423,129000,1292554166690633,101000,1292554167824633,1134000,1292554169398131,1292554169943131,1573498,545000,1292554180884270,1292554187586270,1292554196991270,10941139,6702000,9405000
        -2143831735395280250,11.111000,1,0,1,0,0,1292554130385270,"[NULL]","[NULL]",1292554131192257,1292554131471257,"[NULL]",279000,1292554131865210,1292554131963210,393953,98000,1292554142599210,1292554142790210,10636000,191000,1292554143168210,378000,1292554143550210,1292554143905210,382000,355000,1292554144027633,1292554144126633,122423,99000,1292554144172633,46000,1292554145188633,1016000,1292554146703131,1292554147186131,1514498,483000,1292554158509270,1292554165207270,1292554174691270,11323139,6698000,9484000
        -2143831735395280248,11.111000,1,0,1,0,0,1292554185877270,"[NULL]","[NULL]",1292554186628257,1292554187026257,"[NULL]",398000,1292554187244210,1292554187351210,217953,107000,1292554198200210,1292554198282210,10849000,82000,1292554198448210,166000,1292554198809210,1292554199126210,361000,317000,1292554199293633,1292554199416633,167423,123000,1292554199482633,66000,1292554200540633,1058000,1292554202232131,1292554202775131,1691498,543000,1292554214234270,1292554221192270,1292554230235270,11459139,6958000,9043000
        -2143831735395280246,11.111000,1,0,1,0,0,1292554196968270,"[NULL]","[NULL]",1292554198042257,1292554198404257,"[NULL]",362000,1292554199295210,1292554199405210,890953,110000,1292554209368210,1292554209458210,9963000,90000,1292554209783210,325000,1292554210109210,1292554210447210,326000,338000,1292554210586633,1292554210744633,139423,158000,1292554210805633,61000,1292554211914633,1109000,1292554212678131,1292554213136131,763498,458000,1292554225142270,1292554231869270,1292554241331270,12006139,6727000,9462000
        -2143831735395280244,11.111000,1,0,1,0,0,1292554163682270,"[NULL]","[NULL]",1292554164468257,1292554164861257,"[NULL]",393000,1292554165375210,1292554165502210,513953,127000,1292554176300210,1292554176526210,10798000,226000,1292554176906210,380000,1292554177277210,1292554177579210,371000,302000,1292554177714633,1292554177819633,135423,105000,1292554177866633,47000,1292554178899633,1033000,1292554180264131,1292554180732131,1364498,468000,1292554191858270,1292554198113270,1292554208013270,11126139,6255000,9900000
        -2143831735395280242,11.111000,1,0,1,0,0,1292554174786270,"[NULL]","[NULL]",1292554175708257,1292554176029257,"[NULL]",321000,1292554176727210,1292554176834210,697953,107000,1292554187011210,1292554187099210,10177000,88000,1292554187399210,300000,1292554187782210,1292554188084210,383000,302000,1292554188237633,1292554188386633,153423,149000,1292554188443633,57000,1292554189523633,1080000,1292554191152131,1292554191689131,1628498,537000,1292554203177270,1292554209528270,1292554219116270,11488139,6351000,9588000
        -2143831735395280239,11.111000,1,0,1,0,1,1292554086893270,"[NULL]","[NULL]",1292554086897257,1292554087025257,"[NULL]",128000,1292554088316210,1292554088395210,1290953,79000,1292554097735210,1292554098425210,9340000,690000,1292554098654210,229000,1292554098936210,1292554099173210,282000,237000,1292554099294633,1292554099407633,121423,113000,1292554099469633,62000,1292554100422633,953000,1292554101634131,1292554101998131,1211498,364000,1292554114587270,1292554120289270,1292554130314270,12589139,5702000,10025000
        -2143831735395280229,11.111000,1,0,1,0,0,1292554119302270,"[NULL]","[NULL]",1292554120042257,1292554120369257,"[NULL]",327000,1292554120537210,1292554120631210,167953,94000,1292554131566210,1292554131724210,10935000,158000,1292554132003210,279000,1292554132424210,1292554132933210,421000,509000,1292554133046633,1292554133166633,113423,120000,1292554133262633,96000,1292554134357633,1095000,1292554135827131,1292554136326131,1469498,499000,1292554147582270,1292554154188270,1292554163654270,11256139,6606000,9466000
        -2143831735395280227,11.111000,1,0,1,0,0,1292554097138270,"[NULL]","[NULL]",1292554097987257,1292554098176257,"[NULL]",189000,1292554098543210,1292554098619210,366953,76000,1292554109098210,1292554109249210,10479000,151000,1292554109537210,288000,1292554109843210,1292554110086210,306000,243000,1292554110300633,1292554110419633,214423,119000,1292554110461633,42000,1292554111414633,953000,1292554112146131,1292554112586131,731498,440000,1292554125377270,1292554131616270,1292554141341270,12791139,6239000,9725000
        -2143831735395280226,11.111000,1,0,1,0,0,1292554108216270,"[NULL]","[NULL]",1292554108988257,1292554109310257,"[NULL]",322000,1292554109391210,1292554109491210,80953,100000,1292554120251210,1292554120389210,10760000,138000,1292554120708210,319000,1292554120998210,1292554121193210,290000,195000,1292554121383633,1292554121472633,190423,89000,1292554121514633,42000,1292554122437633,923000,1292554123980131,1292554124406131,1542498,426000,1292554136757270,1292554143046270,1292554152550270,12351139,6289000,9504000
        -2143831735395280208,11.111000,1,0,1,0,0,1292554230251270,"[NULL]","[NULL]",1292554231054257,1292554231462257,"[NULL]",408000,1292554231608210,1292554231711210,145953,103000,1292554242726210,1292554242854210,11015000,128000,1292554243173210,319000,1292554243533210,1292554243752210,360000,219000,1292554244019633,1292554244177633,267423,158000,1292554244238633,61000,1292554245347633,1109000,1292554246815131,1292554247373131,1467498,558000,1292554258496270,1292554265091270,1292554274680270,11123139,6595000,9589000
        -2143831735395280206,11.111000,1,0,1,0,0,1292554241443270,"[NULL]","[NULL]",1292554242336257,1292554242660257,"[NULL]",324000,1292554242996210,1292554243116210,335953,120000,1292554254186210,1292554254324210,11070000,138000,1292554254651210,327000,1292554255081210,1292554255282210,430000,201000,1292554255581633,1292554255710633,299423,129000,1292554255776633,66000,1292554256886633,1110000,1292554258409131,1292554259000131,1522498,591000,1292554269967270,1292554276658270,1292554285774270,10967139,6691000,9116000
        -2143831735395280204,11.111000,1,0,1,0,0,1292554208072270,"[NULL]","[NULL]",1292554208931257,1292554209188257,"[NULL]",257000,1292554209612210,1292554209722210,423953,110000,1292554220579210,1292554220686210,10857000,107000,1292554220890210,204000,1292554221324210,1292554221679210,434000,355000,1292554221876633,1292554222020633,197423,144000,1292554222087633,67000,1292554223557633,1470000,1292554225289131,1292554225947131,1731498,658000,1292554236043270,1292554243002270,1292554252449270,10096139,6959000,9447000
        -2143831735395280202,11.111000,1,0,1,0,0,1292554219159270,"[NULL]","[NULL]",1292554220303257,1292554220678257,"[NULL]",375000,1292554221904210,1292554222054210,1225953,150000,1292554231391210,1292554231468210,9337000,77000,1292554231758210,290000,1292554232098210,1292554232394210,340000,296000,1292554232521633,1292554232675633,127423,154000,1292554232737633,62000,1292554233779633,1042000,1292554235226131,1292554235705131,1446498,479000,1292554247300270,1292554253765270,1292554263549270,11595139,6465000,9784000
        -2143831735395280200,0.000000,0,0,1,0,0,1292554274773270,"[NULL]","[NULL]",1292554275745257,1292554276049257,"[NULL]",304000,1292554276887210,1292554277031210,837953,144000,1292554286887210,1292554287129210,9856000,242000,"[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]"
        -2143831735395280196,11.111000,1,0,1,0,0,1292554252553270,"[NULL]","[NULL]",1292554253301257,1292554253646257,"[NULL]",345000,1292554254466210,1292554254585210,819953,119000,1292554266517210,1292554266634210,11932000,117000,1292554267072210,438000,1292554267538210,1292554267740210,466000,202000,1292554268209633,1292554268405633,469423,196000,1292554268466633,61000,1292554269613633,1147000,1292554271890131,1292554272585131,2276498,695000,1292554280861270,1292554287004270,1292554296910270,8276139,6143000,9906000
        -2143831735395280194,0.000000,0,0,1,0,0,1292554263653270,"[NULL]","[NULL]",1292554264600257,1292554264879257,"[NULL]",279000,1292554266795210,1292554266988210,1915953,193000,1292554276544210,1292554276677210,9556000,133000,"[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]"
        -2143831735395280183,0.000000,0,0,0,0,1,1292554034979270,1292554038935257,3955987,1292554039221257,1292554039362257,286000,141000,1292554039380210,1292554039504210,17953,124000,1292554043444210,1292554043545210,3940000,101000,"[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]"
        -2143831735395280179,11.111000,1,0,0,0,1,1292554029441270,1292554037281257,7839987,1292554037618257,1292554037785257,337000,167000,1292554038237210,1292554038326210,451953,89000,1292554042749210,1292554043429210,4423000,680000,1292554043721210,292000,1292554044172210,1292554044487210,451000,315000,1292554044656633,1292554045326633,169423,670000,1292554047111633,1785000,1292554048159633,1048000,1292554049700131,1292554050158131,1540498,458000,1292554058809270,1292554065670270,1292554074705270,8651139,6861000,9035000
        -2143831735395280166,11.111000,1,0,0,1,1,1292554023976270,1292554027681257,3704987,1292554029847257,1292554030083257,2166000,236000,1292554030360210,1292554030737210,276953,377000,1292554030360210,1292554030725210,-377000,365000,1292554030873210,148000,1292554031441210,1292554031668210,568000,227000,1292554032587633,1292554033341633,919423,754000,"[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]",1292554048008270,1292554053830270,1292554063604270,"[NULL]",5822000,9774000
  """))

  # A trace from M132 (ToT as of adding this test) has the necessary
  # events/arguments (including the ones from the 'view' atrace category).
  def test_chrome_input_dispatch_step(self):
        return DiffTestBlueprint(
        trace=DataPath('scroll_m132_with_atrace.pftrace'),
        query="""
        INCLUDE PERFETTO MODULE chrome.android_input;

        SELECT
          input_reader_processing_start_ts,
          input_reader_processing_end_ts,
          input_reader_utid,
          input_dispatcher_processing_start_ts,
          input_dispatcher_processing_end_ts,
          input_dispatcher_utid,
          deliver_input_event_start_ts,
          deliver_input_event_end_ts,
          deliver_input_event_utid
        FROM chrome_android_input
        WHERE android_input_id = '0x35f0bf2b'
        """,
        out=Csv("""
        "input_reader_processing_start_ts","input_reader_processing_end_ts","input_reader_utid","input_dispatcher_processing_start_ts","input_dispatcher_processing_end_ts","input_dispatcher_utid","deliver_input_event_start_ts","deliver_input_event_end_ts","deliver_input_event_utid"
        1295608261171203,1295608261380838,1404,1295608261495462,1295608262021300,1403,1295608261771463,1295608262613138,7
        """))
