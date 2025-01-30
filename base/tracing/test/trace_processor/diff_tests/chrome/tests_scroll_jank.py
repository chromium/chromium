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
        -2143831735395280183,-2143831735395280179
        -2143831735395280158,-2143831735395280153
        -2143831735395280150,-2143831735395280146
        -2143831735395280144,-2143831735395280139
        -2143831735395280133,-2143831735395280132
        -2143831735395279828,-2143831735395279840
        -2143831735395279804,-2143831735395279784
        -2143831735395279796,-2143831735395279808
        -2143831735395279788,-2143831735395279704
        -2143831735395279780,-2143831735395279792
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

  def test_chrome_touch_move_to_scroll_update_not_forwarded_to_renderer(self):
        return DiffTestBlueprint(
        trace=DataPath('scroll_with_input_not_forwarded_to_renderer.pftrace'),
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
        8456911910253096960,8456911910253096967
        8456911910253096963,8456911910253096962
        8456911910253096965,8456911910253096964
        8456911910253096970,8456911910253096969
        8456911910253096978,8456911910253096977
        8456911910253096980,8456911910253096987
        8456911910253096983,8456911910253096982
        8456911910253096985,8456911910253096984
        8456911910253096990,8456911910253096989
        8456911910253096993,8456911910253096992
        """))

  def test_chrome_scroll_update_refs(self):
        return DiffTestBlueprint(
        trace=DataPath('scroll_m131.pftrace'),
        query="""
        INCLUDE PERFETTO MODULE chrome.chrome_scrolls;

        SELECT
          scroll_update_latency_id,
          touch_move_latency_id,
          presentation_latency_id,
          surface_frame_id,
          display_trace_id
        FROM chrome_scroll_update_refs
        ORDER BY scroll_update_latency_id
        LIMIT 21
        """,
        out=Csv("""
        "scroll_update_latency_id","touch_move_latency_id","presentation_latency_id","surface_frame_id","display_trace_id"
        -2143831735395280256,"[NULL]",-2143831735395280256,1407387768455513,65727
        -2143831735395280254,"[NULL]",-2143831735395280254,1407387768455514,65728
        -2143831735395280250,"[NULL]",-2143831735395280250,1407387768455512,65726
        -2143831735395280248,"[NULL]",-2143831735395280248,1407387768455517,65731
        -2143831735395280246,"[NULL]",-2143831735395280246,1407387768455518,65732
        -2143831735395280244,"[NULL]",-2143831735395280244,1407387768455515,65729
        -2143831735395280242,"[NULL]",-2143831735395280242,1407387768455516,65730
        -2143831735395280239,-2143831735395280236,-2143831735395280239,1407387768455508,65722
        -2143831735395280229,"[NULL]",-2143831735395280229,1407387768455511,65725
        -2143831735395280227,"[NULL]",-2143831735395280227,1407387768455509,65723
        -2143831735395280226,"[NULL]",-2143831735395280226,1407387768455510,65724
        -2143831735395280208,"[NULL]",-2143831735395280208,1407387768455521,65735
        -2143831735395280206,"[NULL]",-2143831735395280206,1407387768455522,65736
        -2143831735395280204,"[NULL]",-2143831735395280204,1407387768455519,65733
        -2143831735395280202,"[NULL]",-2143831735395280202,1407387768455520,65734
        -2143831735395280200,"[NULL]",-2143831735395280200,"[NULL]","[NULL]"
        -2143831735395280196,"[NULL]",-2143831735395280196,1407387768455523,65737
        -2143831735395280194,"[NULL]",-2143831735395280194,"[NULL]","[NULL]"
        -2143831735395280183,-2143831735395280177,-2143831735395280179,"[NULL]","[NULL]"
        -2143831735395280179,-2143831735395280189,-2143831735395280179,1407387768455503,65717
        -2143831735395280166,-2143831735395280163,-2143831735395280166,1407387768455501,"[NULL]"
  """))

  def test_chrome_scroll_update_input_pipeline(self):
        return DiffTestBlueprint(
        trace=DataPath('scroll_m131.pftrace'),
        query="""
        INCLUDE PERFETTO MODULE chrome.chrome_scrolls;

        SELECT
          id,
          presented_in_frame_id,
          is_presented,
          is_janky,
          is_inertial,
          is_first_scroll_update_in_scroll,
          is_first_scroll_update_in_frame,
          generation_ts,
          generation_to_browser_main_dur,
          browser_utid,
          touch_move_received_slice_id,
          touch_move_received_ts,
          touch_move_processing_dur,
          scroll_update_created_slice_id,
          scroll_update_created_ts,
          scroll_update_processing_dur,
          scroll_update_created_end_ts,
          browser_to_compositor_delay_dur,
          compositor_utid,
          compositor_dispatch_slice_id,
          compositor_dispatch_ts,
          compositor_dispatch_dur,
          compositor_dispatch_end_ts,
          compositor_dispatch_to_coalesced_input_handled_dur,
          compositor_coalesced_input_handled_slice_id,
          compositor_coalesced_input_handled_ts,
          compositor_coalesced_input_handled_dur,
          compositor_coalesced_input_handled_end_ts
        FROM chrome_scroll_update_input_pipeline
        ORDER BY id
        LIMIT 21
        """,
        out=Csv("""
        "id","presented_in_frame_id","is_presented","is_janky","is_inertial","is_first_scroll_update_in_scroll","is_first_scroll_update_in_frame","generation_ts","generation_to_browser_main_dur","browser_utid","touch_move_received_slice_id","touch_move_received_ts","touch_move_processing_dur","scroll_update_created_slice_id","scroll_update_created_ts","scroll_update_processing_dur","scroll_update_created_end_ts","browser_to_compositor_delay_dur","compositor_utid","compositor_dispatch_slice_id","compositor_dispatch_ts","compositor_dispatch_dur","compositor_dispatch_end_ts","compositor_dispatch_to_coalesced_input_handled_dur","compositor_coalesced_input_handled_slice_id","compositor_coalesced_input_handled_ts","compositor_coalesced_input_handled_dur","compositor_coalesced_input_handled_end_ts"
        -2143831735395280256,-2143831735395280256,1,0,1,0,1,1292554141489270,677987,1,"[NULL]","[NULL]","[NULL]",10781,1292554142167257,363000,1292554142530257,472953,4,10796,1292554143003210,108000,1292554143111210,10912000,10827,1292554154023210,83000,1292554154106210
        -2143831735395280254,-2143831735395280254,1,0,1,0,1,1292554152575270,1654987,1,"[NULL]","[NULL]","[NULL]",10830,1292554154230257,259000,1292554154489257,698953,4,10845,1292554155188210,120000,1292554155308210,9637000,10869,1292554164945210,223000,1292554165168210
        -2143831735395280250,-2143831735395280250,1,0,1,0,1,1292554130385270,806987,1,"[NULL]","[NULL]","[NULL]",10742,1292554131192257,279000,1292554131471257,393953,4,10757,1292554131865210,98000,1292554131963210,10636000,10790,1292554142599210,191000,1292554142790210
        -2143831735395280248,-2143831735395280248,1,0,1,0,1,1292554185877270,750987,1,"[NULL]","[NULL]","[NULL]",10939,1292554186628257,398000,1292554187026257,217953,4,10950,1292554187244210,107000,1292554187351210,10849000,10988,1292554198200210,82000,1292554198282210
        -2143831735395280246,-2143831735395280246,1,0,1,0,1,1292554196968270,1073987,1,"[NULL]","[NULL]","[NULL]",10980,1292554198042257,362000,1292554198404257,890953,4,11000,1292554199295210,110000,1292554199405210,9963000,11025,1292554209368210,90000,1292554209458210
        -2143831735395280244,-2143831735395280244,1,0,1,0,1,1292554163682270,785987,1,"[NULL]","[NULL]","[NULL]",10860,1292554164468257,393000,1292554164861257,513953,4,10876,1292554165375210,127000,1292554165502210,10798000,10908,1292554176300210,226000,1292554176526210
        -2143831735395280242,-2143831735395280242,1,0,1,0,1,1292554174786270,921987,1,"[NULL]","[NULL]","[NULL]",10899,1292554175708257,321000,1292554176029257,697953,4,10915,1292554176727210,107000,1292554176834210,10177000,10947,1292554187011210,88000,1292554187099210
        -2143831735395280239,-2143831735395280239,1,0,1,0,1,1292554086893270,3987,1,"[NULL]","[NULL]","[NULL]",10555,1292554086897257,128000,1292554087025257,1290953,4,10586,1292554088316210,79000,1292554088395210,9853000,10620,1292554098248210,177000,1292554098425210
        -2143831735395280229,-2143831735395280229,1,0,1,0,1,1292554119302270,739987,1,"[NULL]","[NULL]","[NULL]",10699,1292554120042257,327000,1292554120369257,167953,4,10714,1292554120537210,94000,1292554120631210,10935000,10750,1292554131566210,158000,1292554131724210
        -2143831735395280227,-2143831735395280227,1,0,1,0,1,1292554097138270,848987,1,"[NULL]","[NULL]","[NULL]",10611,1292554097987257,189000,1292554098176257,366953,4,10626,1292554098543210,76000,1292554098619210,10479000,10662,1292554109098210,151000,1292554109249210
        -2143831735395280226,-2143831735395280226,1,0,1,0,1,1292554108216270,771987,1,"[NULL]","[NULL]","[NULL]",10657,1292554108988257,322000,1292554109310257,80953,4,10666,1292554109391210,100000,1292554109491210,10760000,10706,1292554120251210,138000,1292554120389210
        -2143831735395280208,-2143831735395280208,1,0,1,0,1,1292554230251270,802987,1,"[NULL]","[NULL]","[NULL]",11096,1292554231054257,408000,1292554231462257,145953,4,11106,1292554231608210,103000,1292554231711210,11015000,11142,1292554242726210,128000,1292554242854210
        -2143831735395280206,-2143831735395280206,1,0,1,0,1,1292554241443270,892987,1,"[NULL]","[NULL]","[NULL]",11134,1292554242336257,324000,1292554242660257,335953,4,11148,1292554242996210,120000,1292554243116210,11070000,11184,1292554254186210,138000,1292554254324210
        -2143831735395280204,-2143831735395280204,1,0,1,0,1,1292554208072270,858987,1,"[NULL]","[NULL]","[NULL]",11017,1292554208931257,257000,1292554209188257,423953,4,11031,1292554209612210,110000,1292554209722210,10857000,11064,1292554220579210,107000,1292554220686210
        -2143831735395280202,-2143831735395280202,1,0,1,0,1,1292554219159270,1143987,1,"[NULL]","[NULL]","[NULL]",11057,1292554220303257,375000,1292554220678257,1225953,4,11078,1292554221904210,150000,1292554222054210,9337000,11103,1292554231391210,77000,1292554231468210
        -2143831735395280200,-2143831735395280200,0,0,1,0,1,1292554274773270,971987,1,"[NULL]","[NULL]","[NULL]",11250,1292554275745257,304000,1292554276049257,837953,4,11266,1292554276887210,144000,1292554277031210,9856000,11290,1292554286887210,242000,1292554287129210
        -2143831735395280196,-2143831735395280196,1,0,1,0,1,1292554252553270,747987,1,"[NULL]","[NULL]","[NULL]",11172,1292554253301257,345000,1292554253646257,819953,4,11187,1292554254466210,119000,1292554254585210,11932000,11223,1292554266517210,117000,1292554266634210
        -2143831735395280194,-2143831735395280194,0,0,1,0,1,1292554263653270,946987,1,"[NULL]","[NULL]","[NULL]",11211,1292554264600257,279000,1292554264879257,1915953,4,11227,1292554266795210,193000,1292554266988210,9556000,11259,1292554276544210,133000,1292554276677210
        -2143831735395280183,-2143831735395280179,0,0,0,0,0,1292554034979270,3955987,1,10192,1292554038935257,286000,10197,1292554039221257,141000,1292554039362257,17953,4,10210,1292554039380210,124000,1292554039504210,3940000,10230,1292554043444210,101000,1292554043545210
        -2143831735395280179,-2143831735395280179,1,0,0,0,1,1292554029441270,7839987,1,10172,1292554037281257,337000,10177,1292554037618257,167000,1292554037785257,451953,4,10189,1292554038237210,89000,1292554038326210,4800000,10229,1292554043126210,303000,1292554043429210
        -2143831735395280166,-2143831735395280166,1,0,0,1,1,1292554023976270,3704987,1,10071,1292554027681257,2166000,10102,1292554029847257,236000,1292554030083257,276953,4,10123,1292554030360210,377000,1292554030737210,-68000,10128,1292554030669210,56000,1292554030725210
        """))

  def test_chrome_scroll_update_frame_pipeline(self):
        return DiffTestBlueprint(
        trace=DataPath('scroll_m131.pftrace'),
        query="""
        INCLUDE PERFETTO MODULE chrome.chrome_scrolls;

        SELECT
          id,
          vsync_interval_ms,
          compositor_resample_slice_id,
          compositor_resample_ts,
          compositor_generate_compositor_frame_slice_id,
          compositor_generate_compositor_frame_ts,
          compositor_generate_frame_to_submit_frame_dur,
          compositor_submit_compositor_frame_slice_id,
          compositor_submit_compositor_frame_ts,
          compositor_submit_frame_dur,
          compositor_submit_compositor_frame_end_ts,
          compositor_to_viz_delay_dur,
          viz_compositor_utid,
          viz_receive_compositor_frame_slice_id,
          viz_receive_compositor_frame_ts,
          viz_receive_compositor_frame_dur,
          viz_receive_compositor_frame_end_ts,
          viz_wait_for_draw_dur,
          viz_draw_and_swap_slice_id,
          viz_draw_and_swap_ts,
          viz_draw_and_swap_dur,
          viz_send_buffer_swap_slice_id,
          viz_send_buffer_swap_end_ts,
          viz_to_gpu_delay_dur,
          viz_gpu_thread_utid,
          viz_swap_buffers_slice_id,
          viz_swap_buffers_ts,
          viz_swap_buffers_dur,
          viz_swap_buffers_end_ts,
          viz_swap_buffers_to_latch_dur,
          latch_timestamp,
          viz_latch_to_presentation_dur,
          presentation_timestamp
        FROM chrome_scroll_update_frame_pipeline
        ORDER BY id
        LIMIT 21
        """,
        out=Csv("""
        "id","vsync_interval_ms","compositor_resample_slice_id","compositor_resample_ts","compositor_generate_compositor_frame_slice_id","compositor_generate_compositor_frame_ts","compositor_generate_frame_to_submit_frame_dur","compositor_submit_compositor_frame_slice_id","compositor_submit_compositor_frame_ts","compositor_submit_frame_dur","compositor_submit_compositor_frame_end_ts","compositor_to_viz_delay_dur","viz_compositor_utid","viz_receive_compositor_frame_slice_id","viz_receive_compositor_frame_ts","viz_receive_compositor_frame_dur","viz_receive_compositor_frame_end_ts","viz_wait_for_draw_dur","viz_draw_and_swap_slice_id","viz_draw_and_swap_ts","viz_draw_and_swap_dur","viz_send_buffer_swap_slice_id","viz_send_buffer_swap_end_ts","viz_to_gpu_delay_dur","viz_gpu_thread_utid","viz_swap_buffers_slice_id","viz_swap_buffers_ts","viz_swap_buffers_dur","viz_swap_buffers_end_ts","viz_swap_buffers_to_latch_dur","latch_timestamp","viz_latch_to_presentation_dur","presentation_timestamp"
        -2143831735395280256,11.111000,"[NULL]","[NULL]",10834,1292554154282210,337000,10838,1292554154619210,337000,1292554154956210,139423,6,10840,1292554155095633,126000,1292554155221633,65000,10846,1292554155286633,1295000,10849,1292554156581633,1620498,7,10850,1292554158202131,536000,1292554158738131,10898139,1292554169636270,16163000,1292554185799270
        -2143831735395280254,11.111000,"[NULL]","[NULL]",10878,1292554165562210,387000,10881,1292554165949210,363000,1292554166312210,148423,6,10882,1292554166460633,129000,1292554166589633,101000,10885,1292554166690633,1134000,10888,1292554167824633,1573498,7,10889,1292554169398131,545000,1292554169943131,10941139,1292554180884270,16107000,1292554196991270
        -2143831735395280250,11.111000,"[NULL]","[NULL]",10799,1292554143168210,382000,10802,1292554143550210,355000,1292554143905210,122423,6,10803,1292554144027633,99000,1292554144126633,46000,10806,1292554144172633,1016000,10809,1292554145188633,1514498,7,10810,1292554146703131,483000,1292554147186131,11323139,1292554158509270,16182000,1292554174691270
        -2143831735395280248,11.111000,"[NULL]","[NULL]",10991,1292554198448210,361000,10995,1292554198809210,317000,1292554199126210,167423,6,10996,1292554199293633,123000,1292554199416633,66000,11002,1292554199482633,1058000,11005,1292554200540633,1691498,7,11006,1292554202232131,543000,1292554202775131,11459139,1292554214234270,16001000,1292554230235270
        -2143831735395280246,11.111000,"[NULL]","[NULL]",11033,1292554209783210,326000,11037,1292554210109210,338000,1292554210447210,139423,6,11038,1292554210586633,158000,1292554210744633,61000,11041,1292554210805633,1109000,11044,1292554211914633,763498,7,11045,1292554212678131,458000,1292554213136131,12006139,1292554225142270,16189000,1292554241331270
        -2143831735395280244,11.111000,"[NULL]","[NULL]",10917,1292554176906210,371000,10920,1292554177277210,302000,1292554177579210,135423,6,10921,1292554177714633,105000,1292554177819633,47000,10924,1292554177866633,1033000,10927,1292554178899633,1364498,7,10928,1292554180264131,468000,1292554180732131,11126139,1292554191858270,16155000,1292554208013270
        -2143831735395280242,11.111000,"[NULL]","[NULL]",10953,1292554187399210,383000,10959,1292554187782210,302000,1292554188084210,153423,6,10960,1292554188237633,149000,1292554188386633,57000,10963,1292554188443633,1080000,10966,1292554189523633,1628498,7,10967,1292554191152131,537000,1292554191689131,11488139,1292554203177270,15939000,1292554219116270
        -2143831735395280239,11.111000,10616,1292554097735210,10629,1292554098654210,282000,10632,1292554098936210,237000,1292554099173210,121423,6,10633,1292554099294633,113000,1292554099407633,62000,10636,1292554099469633,953000,10641,1292554100422633,1211498,7,10643,1292554101634131,364000,1292554101998131,12589139,1292554114587270,15727000,1292554130314270
        -2143831735395280229,11.111000,"[NULL]","[NULL]",10759,1292554132003210,421000,10762,1292554132424210,509000,1292554132933210,113423,6,10763,1292554133046633,120000,1292554133166633,96000,10766,1292554133262633,1095000,10769,1292554134357633,1469498,7,10770,1292554135827131,499000,1292554136326131,11256139,1292554147582270,16072000,1292554163654270
        -2143831735395280227,11.111000,"[NULL]","[NULL]",10669,1292554109537210,306000,10677,1292554109843210,243000,1292554110086210,214423,6,10679,1292554110300633,119000,1292554110419633,42000,10682,1292554110461633,953000,10685,1292554111414633,731498,7,10686,1292554112146131,440000,1292554112586131,12791139,1292554125377270,15964000,1292554141341270
        -2143831735395280226,11.111000,"[NULL]","[NULL]",10716,1292554120708210,290000,10721,1292554120998210,195000,1292554121193210,190423,6,10724,1292554121383633,89000,1292554121472633,42000,10727,1292554121514633,923000,10730,1292554122437633,1542498,7,10731,1292554123980131,426000,1292554124406131,12351139,1292554136757270,15793000,1292554152550270
        -2143831735395280208,11.111000,"[NULL]","[NULL]",11150,1292554243173210,360000,11153,1292554243533210,219000,1292554243752210,267423,6,11154,1292554244019633,158000,1292554244177633,61000,11157,1292554244238633,1109000,11161,1292554245347633,1467498,7,11162,1292554246815131,558000,1292554247373131,11123139,1292554258496270,16184000,1292554274680270
        -2143831735395280206,11.111000,"[NULL]","[NULL]",11189,1292554254651210,430000,11192,1292554255081210,201000,1292554255282210,299423,6,11193,1292554255581633,129000,1292554255710633,66000,11197,1292554255776633,1110000,11200,1292554256886633,1522498,7,11201,1292554258409131,591000,1292554259000131,10967139,1292554269967270,15807000,1292554285774270
        -2143831735395280204,11.111000,"[NULL]","[NULL]",11066,1292554220890210,434000,11073,1292554221324210,355000,1292554221679210,197423,6,11074,1292554221876633,144000,1292554222020633,67000,11080,1292554222087633,1470000,11083,1292554223557633,1731498,7,11085,1292554225289131,658000,1292554225947131,10096139,1292554236043270,16406000,1292554252449270
        -2143831735395280202,11.111000,"[NULL]","[NULL]",11109,1292554231758210,340000,11115,1292554232098210,296000,1292554232394210,127423,6,11116,1292554232521633,154000,1292554232675633,62000,11119,1292554232737633,1042000,11122,1292554233779633,1446498,7,11123,1292554235226131,479000,1292554235705131,11595139,1292554247300270,16249000,1292554263549270
        -2143831735395280200,0.000000,"[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]"
        -2143831735395280196,11.111000,"[NULL]","[NULL]",11229,1292554267072210,466000,11232,1292554267538210,202000,1292554267740210,469423,6,11233,1292554268209633,196000,1292554268405633,61000,11236,1292554268466633,1147000,11239,1292554269613633,2276498,7,11241,1292554271890131,695000,1292554272585131,8276139,1292554280861270,16049000,1292554296910270
        -2143831735395280194,0.000000,"[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]"
        -2143831735395280179,11.111000,10223,1292554042749210,10233,1292554043721210,451000,10239,1292554044172210,315000,1292554044487210,169423,6,10245,1292554044656633,670000,1292554045326633,1785000,10266,1292554047111633,1048000,10271,1292554048159633,1540498,7,10272,1292554049700131,458000,1292554050158131,8651139,1292554058809270,15896000,1292554074705270
        -2143831735395280166,11.111000,10124,1292554030360210,10130,1292554030873210,568000,10135,1292554031441210,227000,1292554031668210,919423,6,10141,1292554032587633,754000,1292554033341633,"[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]",1292554048008270,15596000,1292554063604270
        -2143831735395280153,11.111000,10469,1292554075561210,10481,1292554076592210,334000,10488,1292554076926210,301000,1292554077227210,177423,6,10494,1292554077404633,138000,1292554077542633,280000,10506,1292554077822633,988000,10509,1292554078810633,1377498,7,10516,1292554080188131,494000,1292554080682131,11265139,1292554091947270,16096000,1292554108043270
        """))

  def test_chrome_scroll_frame_info(self):
        return DiffTestBlueprint(
        trace=DataPath('scroll_m132.pftrace'),
        query="""
        INCLUDE PERFETTO MODULE chrome.chrome_scrolls;

        WITH
        -- Filter out last frames as the trace finished earlier then when they have been
        -- presented.
        frames AS (
          SELECT * FROM chrome_scroll_frame_info
          WHERE first_input_generation_ts <= 3030301718162132
        )
        SELECT
          (SELECT COUNT(*) FROM frames) AS frame_count,
          -- crbug.com/380286381: EventLatencies with slice ids 14862, 14937,
          -- 14987 are presented at the same time as EventLatency 14768 and
          -- are filtered out since they are not coalesced and don't have
          -- a frame_display_id.
          (SELECT COUNT(DISTINCT id) FROM frames) AS unique_frame_count
        """,
        out=Csv("""
        "frame_count","unique_frame_count"
        259,259
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
          is_first_scroll_update_in_scroll,
          is_first_scroll_update_in_frame,
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
          presentation_timestamp,
          viz_swap_buffers_to_latch_dur,
          viz_latch_to_presentation_dur
        FROM chrome_scroll_update_info
        ORDER BY id
        LIMIT 21
        """,
        out=Csv("""
        "id","vsync_interval_ms","is_presented","is_janky","is_inertial","is_first_scroll_update_in_scroll","is_first_scroll_update_in_frame","generation_ts","touch_move_received_ts","generation_to_browser_main_dur","scroll_update_created_ts","scroll_update_created_end_ts","touch_move_processing_dur","scroll_update_processing_dur","compositor_dispatch_ts","compositor_dispatch_end_ts","browser_to_compositor_delay_dur","compositor_dispatch_dur","compositor_on_begin_frame_ts","compositor_on_begin_frame_end_ts","compositor_dispatch_to_on_begin_frame_delay_dur","compositor_on_begin_frame_dur","compositor_generate_compositor_frame_ts","compositor_on_begin_frame_to_generation_delay_dur","compositor_submit_compositor_frame_ts","compositor_submit_compositor_frame_end_ts","compositor_generate_frame_to_submit_frame_dur","compositor_submit_frame_dur","viz_receive_compositor_frame_ts","viz_receive_compositor_frame_end_ts","compositor_to_viz_delay_dur","viz_receive_compositor_frame_dur","viz_draw_and_swap_ts","viz_wait_for_draw_dur","viz_send_buffer_swap_end_ts","viz_draw_and_swap_dur","viz_swap_buffers_ts","viz_swap_buffers_end_ts","viz_to_gpu_delay_dur","viz_swap_buffers_dur","latch_timestamp","presentation_timestamp","viz_swap_buffers_to_latch_dur","viz_latch_to_presentation_dur"
        -2143831735395280256,11.111000,1,0,1,0,1,1292554141489270,"[NULL]",677987,1292554142167257,1292554142530257,"[NULL]",363000,1292554143003210,1292554143111210,472953,108000,1292554154023210,1292554154106210,10912000,83000,1292554154282210,176000,1292554154619210,1292554154956210,337000,337000,1292554155095633,1292554155221633,139423,126000,1292554155286633,65000,1292554156581633,1295000,1292554158202131,1292554158738131,1620498,536000,1292554169636270,1292554185799270,10898139,16163000
        -2143831735395280254,11.111000,1,0,1,0,1,1292554152575270,"[NULL]",1654987,1292554154230257,1292554154489257,"[NULL]",259000,1292554155188210,1292554155308210,698953,120000,1292554164945210,1292554165168210,9637000,223000,1292554165562210,394000,1292554165949210,1292554166312210,387000,363000,1292554166460633,1292554166589633,148423,129000,1292554166690633,101000,1292554167824633,1134000,1292554169398131,1292554169943131,1573498,545000,1292554180884270,1292554196991270,10941139,16107000
        -2143831735395280250,11.111000,1,0,1,0,1,1292554130385270,"[NULL]",806987,1292554131192257,1292554131471257,"[NULL]",279000,1292554131865210,1292554131963210,393953,98000,1292554142599210,1292554142790210,10636000,191000,1292554143168210,378000,1292554143550210,1292554143905210,382000,355000,1292554144027633,1292554144126633,122423,99000,1292554144172633,46000,1292554145188633,1016000,1292554146703131,1292554147186131,1514498,483000,1292554158509270,1292554174691270,11323139,16182000
        -2143831735395280248,11.111000,1,0,1,0,1,1292554185877270,"[NULL]",750987,1292554186628257,1292554187026257,"[NULL]",398000,1292554187244210,1292554187351210,217953,107000,1292554198200210,1292554198282210,10849000,82000,1292554198448210,166000,1292554198809210,1292554199126210,361000,317000,1292554199293633,1292554199416633,167423,123000,1292554199482633,66000,1292554200540633,1058000,1292554202232131,1292554202775131,1691498,543000,1292554214234270,1292554230235270,11459139,16001000
        -2143831735395280246,11.111000,1,0,1,0,1,1292554196968270,"[NULL]",1073987,1292554198042257,1292554198404257,"[NULL]",362000,1292554199295210,1292554199405210,890953,110000,1292554209368210,1292554209458210,9963000,90000,1292554209783210,325000,1292554210109210,1292554210447210,326000,338000,1292554210586633,1292554210744633,139423,158000,1292554210805633,61000,1292554211914633,1109000,1292554212678131,1292554213136131,763498,458000,1292554225142270,1292554241331270,12006139,16189000
        -2143831735395280244,11.111000,1,0,1,0,1,1292554163682270,"[NULL]",785987,1292554164468257,1292554164861257,"[NULL]",393000,1292554165375210,1292554165502210,513953,127000,1292554176300210,1292554176526210,10798000,226000,1292554176906210,380000,1292554177277210,1292554177579210,371000,302000,1292554177714633,1292554177819633,135423,105000,1292554177866633,47000,1292554178899633,1033000,1292554180264131,1292554180732131,1364498,468000,1292554191858270,1292554208013270,11126139,16155000
        -2143831735395280242,11.111000,1,0,1,0,1,1292554174786270,"[NULL]",921987,1292554175708257,1292554176029257,"[NULL]",321000,1292554176727210,1292554176834210,697953,107000,1292554187011210,1292554187099210,10177000,88000,1292554187399210,300000,1292554187782210,1292554188084210,383000,302000,1292554188237633,1292554188386633,153423,149000,1292554188443633,57000,1292554189523633,1080000,1292554191152131,1292554191689131,1628498,537000,1292554203177270,1292554219116270,11488139,15939000
        -2143831735395280239,11.111000,1,0,1,0,1,1292554086893270,"[NULL]",3987,1292554086897257,1292554087025257,"[NULL]",128000,1292554088316210,1292554088395210,1290953,79000,1292554097735210,1292554098425210,9340000,690000,1292554098654210,229000,1292554098936210,1292554099173210,282000,237000,1292554099294633,1292554099407633,121423,113000,1292554099469633,62000,1292554100422633,953000,1292554101634131,1292554101998131,1211498,364000,1292554114587270,1292554130314270,12589139,15727000
        -2143831735395280229,11.111000,1,0,1,0,1,1292554119302270,"[NULL]",739987,1292554120042257,1292554120369257,"[NULL]",327000,1292554120537210,1292554120631210,167953,94000,1292554131566210,1292554131724210,10935000,158000,1292554132003210,279000,1292554132424210,1292554132933210,421000,509000,1292554133046633,1292554133166633,113423,120000,1292554133262633,96000,1292554134357633,1095000,1292554135827131,1292554136326131,1469498,499000,1292554147582270,1292554163654270,11256139,16072000
        -2143831735395280227,11.111000,1,0,1,0,1,1292554097138270,"[NULL]",848987,1292554097987257,1292554098176257,"[NULL]",189000,1292554098543210,1292554098619210,366953,76000,1292554109098210,1292554109249210,10479000,151000,1292554109537210,288000,1292554109843210,1292554110086210,306000,243000,1292554110300633,1292554110419633,214423,119000,1292554110461633,42000,1292554111414633,953000,1292554112146131,1292554112586131,731498,440000,1292554125377270,1292554141341270,12791139,15964000
        -2143831735395280226,11.111000,1,0,1,0,1,1292554108216270,"[NULL]",771987,1292554108988257,1292554109310257,"[NULL]",322000,1292554109391210,1292554109491210,80953,100000,1292554120251210,1292554120389210,10760000,138000,1292554120708210,319000,1292554120998210,1292554121193210,290000,195000,1292554121383633,1292554121472633,190423,89000,1292554121514633,42000,1292554122437633,923000,1292554123980131,1292554124406131,1542498,426000,1292554136757270,1292554152550270,12351139,15793000
        -2143831735395280208,11.111000,1,0,1,0,1,1292554230251270,"[NULL]",802987,1292554231054257,1292554231462257,"[NULL]",408000,1292554231608210,1292554231711210,145953,103000,1292554242726210,1292554242854210,11015000,128000,1292554243173210,319000,1292554243533210,1292554243752210,360000,219000,1292554244019633,1292554244177633,267423,158000,1292554244238633,61000,1292554245347633,1109000,1292554246815131,1292554247373131,1467498,558000,1292554258496270,1292554274680270,11123139,16184000
        -2143831735395280206,11.111000,1,0,1,0,1,1292554241443270,"[NULL]",892987,1292554242336257,1292554242660257,"[NULL]",324000,1292554242996210,1292554243116210,335953,120000,1292554254186210,1292554254324210,11070000,138000,1292554254651210,327000,1292554255081210,1292554255282210,430000,201000,1292554255581633,1292554255710633,299423,129000,1292554255776633,66000,1292554256886633,1110000,1292554258409131,1292554259000131,1522498,591000,1292554269967270,1292554285774270,10967139,15807000
        -2143831735395280204,11.111000,1,0,1,0,1,1292554208072270,"[NULL]",858987,1292554208931257,1292554209188257,"[NULL]",257000,1292554209612210,1292554209722210,423953,110000,1292554220579210,1292554220686210,10857000,107000,1292554220890210,204000,1292554221324210,1292554221679210,434000,355000,1292554221876633,1292554222020633,197423,144000,1292554222087633,67000,1292554223557633,1470000,1292554225289131,1292554225947131,1731498,658000,1292554236043270,1292554252449270,10096139,16406000
        -2143831735395280202,11.111000,1,0,1,0,1,1292554219159270,"[NULL]",1143987,1292554220303257,1292554220678257,"[NULL]",375000,1292554221904210,1292554222054210,1225953,150000,1292554231391210,1292554231468210,9337000,77000,1292554231758210,290000,1292554232098210,1292554232394210,340000,296000,1292554232521633,1292554232675633,127423,154000,1292554232737633,62000,1292554233779633,1042000,1292554235226131,1292554235705131,1446498,479000,1292554247300270,1292554263549270,11595139,16249000
        -2143831735395280200,0.000000,0,0,1,0,1,1292554274773270,"[NULL]",971987,1292554275745257,1292554276049257,"[NULL]",304000,1292554276887210,1292554277031210,837953,144000,1292554286887210,1292554287129210,9856000,242000,"[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]"
        -2143831735395280196,11.111000,1,0,1,0,1,1292554252553270,"[NULL]",747987,1292554253301257,1292554253646257,"[NULL]",345000,1292554254466210,1292554254585210,819953,119000,1292554266517210,1292554266634210,11932000,117000,1292554267072210,438000,1292554267538210,1292554267740210,466000,202000,1292554268209633,1292554268405633,469423,196000,1292554268466633,61000,1292554269613633,1147000,1292554271890131,1292554272585131,2276498,695000,1292554280861270,1292554296910270,8276139,16049000
        -2143831735395280194,0.000000,0,0,1,0,1,1292554263653270,"[NULL]",946987,1292554264600257,1292554264879257,"[NULL]",279000,1292554266795210,1292554266988210,1915953,193000,1292554276544210,1292554276677210,9556000,133000,"[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]"
        -2143831735395280183,11.111000,0,0,0,0,0,1292554034979270,1292554038935257,3955987,1292554039221257,1292554039362257,286000,141000,1292554039380210,1292554039504210,17953,124000,1292554042749210,1292554043545210,3245000,796000,1292554043721210,176000,1292554044172210,1292554044487210,451000,315000,1292554044656633,1292554045326633,169423,670000,1292554047111633,1785000,1292554048159633,1048000,1292554049700131,1292554050158131,1540498,458000,1292554058809270,1292554074705270,8651139,15896000
        -2143831735395280179,11.111000,1,0,0,0,1,1292554029441270,1292554037281257,7839987,1292554037618257,1292554037785257,337000,167000,1292554038237210,1292554038326210,451953,89000,1292554042749210,1292554043429210,4423000,680000,1292554043721210,292000,1292554044172210,1292554044487210,451000,315000,1292554044656633,1292554045326633,169423,670000,1292554047111633,1785000,1292554048159633,1048000,1292554049700131,1292554050158131,1540498,458000,1292554058809270,1292554074705270,8651139,15896000
        -2143831735395280166,11.111000,1,0,0,1,1,1292554023976270,1292554027681257,3704987,1292554029847257,1292554030083257,2166000,236000,1292554030360210,1292554030737210,276953,377000,1292554030360210,1292554030725210,-377000,365000,1292554030873210,148000,1292554031441210,1292554031668210,568000,227000,1292554032587633,1292554033341633,919423,754000,"[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]","[NULL]",1292554048008270,1292554063604270,"[NULL]",15596000
        """))

  def test_chrome_scroll_update_info_step_templates(self):
        # Verify that chrome_scroll_update_info_step_templates references at
        # least one valid column name and no invalid column names in
        # chrome_scroll_update_info.
        return DiffTestBlueprint(
        trace=DataPath('scroll_m131.pftrace'),
        query="""
        INCLUDE PERFETTO MODULE chrome.chrome_scrolls;

        WITH referenced_column_names AS (
          SELECT
            ts_column_name AS column_name
          FROM chrome_scroll_update_info_step_templates
          WHERE column_name IS NOT NULL
          UNION ALL
          SELECT
            dur_column_name AS column_name
          FROM chrome_scroll_update_info_step_templates
          WHERE column_name IS NOT NULL
        ),
        valid_column_names AS (
          SELECT name AS column_name
          FROM pragma_table_info('chrome_scroll_update_info')
        )
        SELECT
          "valid" AS validity,
          EXISTS (
            SELECT column_name FROM referenced_column_names
            WHERE column_name IN valid_column_names
          ) AS existence
        UNION ALL
        SELECT
          "invalid" AS validity,
          EXISTS (
            SELECT column_name FROM referenced_column_names
            WHERE column_name NOT IN valid_column_names
          ) AS existence
        ORDER BY validity DESC;
        """,
        out=Csv("""
        "validity","existence"
        "valid",1
        "invalid",0
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
