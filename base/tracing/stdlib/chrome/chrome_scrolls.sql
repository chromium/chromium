-- Copyright 2023 The Chromium Authors
-- Use of this source code is governed by a BSD-style license that can be
-- found in the LICENSE file.

INCLUDE PERFETTO MODULE chrome.event_latency;
INCLUDE PERFETTO MODULE chrome.graphics_pipeline;
INCLUDE PERFETTO MODULE chrome.input;
INCLUDE PERFETTO MODULE chrome.scroll_jank.utils;

-- Ties together input (`LatencyInfo.Flow`) and frame (`Graphics.Pipeline`)
-- trace events. Only covers input events of the `GESTURE_SCROLL_UPDATE_EVENT`
-- type.
CREATE PERFETTO TABLE _chrome_scroll_update_refs(
  -- Id of the Chrome input pipeline (`LatencyInfo.Flow`).
  scroll_update_latency_id INT,
  -- Id of the touch move input corresponding to this scroll update.
  touch_move_latency_id INT,
  -- Id of the frame pipeline (`Graphics.Pipeline`), pre-surface aggregation.
  surface_frame_id INT,
  -- Id of the frame pipeline (`Graphics.Pipeline`), post-surface aggregation.
  display_trace_id INT)
AS
SELECT
  scroll_update.latency_id AS scroll_update_latency_id,
  chrome_touch_move_to_scroll_update.touch_move_latency_id,
  chrome_graphics_pipeline_inputs_to_surface_frames.surface_frame_trace_id
    AS surface_frame_id,
  chrome_graphics_pipeline_aggregated_frames.display_trace_id
FROM
  chrome_inputs scroll_update
LEFT JOIN chrome_graphics_pipeline_inputs_to_surface_frames
  USING (latency_id)
LEFT JOIN chrome_graphics_pipeline_aggregated_frames
  ON
    chrome_graphics_pipeline_aggregated_frames.surface_frame_trace_id
    = chrome_graphics_pipeline_inputs_to_surface_frames.surface_frame_trace_id
LEFT JOIN chrome_touch_move_to_scroll_update
  ON
    chrome_touch_move_to_scroll_update.scroll_update_latency_id
    = scroll_update.latency_id
WHERE scroll_update.input_type = 'GESTURE_SCROLL_UPDATE_EVENT';

-- Timestamps and other related information for events during the critical path
-- for scrolling.
CREATE PERFETTO TABLE _scroll_update_timestamps_and_metadata
AS
SELECT
  refs.scroll_update_latency_id AS id,
  chrome_coalesced_input.presented_latency_id AS coalesced_into,
  -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
  chrome_event_latency.vsync_interval_ms AS vsync_interval_ms,
  chrome_event_latency.is_presented AS is_presented,
  chrome_event_latency.is_janky_scrolled_frame AS is_janky,
  chrome_event_latency.event_type
    = 'INERTIAL_GESTURE_SCROLL_UPDATE' AS is_inertial,
  chrome_event_latency.ts AS generation_ts,
  -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
  touch_move_received_step.slice_id AS touch_move_received_slice_id,
  touch_move_received_step.ts AS touch_move_received_ts,
  -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
  touch_move_processed_step.slice_id AS touch_move_processed_slice_id,
  touch_move_processed_step.ts AS touch_move_processed_ts,
  -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
  scroll_update_created_step.slice_id AS scroll_update_created_slice_id,
  scroll_update_created_step.utid AS browser_utid,
  scroll_update_created_step.ts AS scroll_update_created_ts,
  scroll_update_created_step.ts + scroll_update_created_step.dur
    AS scroll_update_created_end_ts,
  -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
  compositor_dispatch_step.slice_id AS compositor_dispatch_slice_id,
  compositor_dispatch_step.task_start_time_ts
    AS compositor_dispatch_task_ts,
  compositor_dispatch_step.ts AS compositor_dispatch_ts,
  compositor_dispatch_step.ts + compositor_dispatch_step.dur
    AS compositor_dispatch_end_ts,
  compositor_dispatch_step.utid AS compositor_utid,
  -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
  compositor_resample_step.slice_id AS compositor_resample_slice_id,
  compositor_resample_step.task_start_time_ts
    AS compositor_resample_task_ts,
  compositor_resample_step.ts AS compositor_resample_ts,
  -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
  compositor_coalesced_input_handled_step.slice_id
    AS compositor_coalesced_input_handled_slice_id,
  compositor_coalesced_input_handled_step.ts
    AS compositor_coalesced_input_handled_ts,
  compositor_coalesced_input_handled_step.ts
    + compositor_coalesced_input_handled_step.dur
    AS compositor_coalesced_input_handled_end_ts,
  -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
  compositor_generate_compositor_frame_step.id
    AS compositor_generate_compositor_frame_slice_id,
  compositor_generate_compositor_frame_step.task_start_time_ts
    AS compositor_generate_compositor_frame_task_ts,
  compositor_generate_compositor_frame_step.ts
    AS compositor_generate_compositor_frame_ts,
  -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
  compositor_submit_compositor_frame_step.id
    AS compositor_submit_compositor_frame_slice_id,
  compositor_submit_compositor_frame_step.ts
    AS compositor_submit_compositor_frame_ts,
  compositor_submit_compositor_frame_step.ts
    + compositor_submit_compositor_frame_step.dur
    AS compositor_submit_compositor_frame_end_ts,
  -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
  viz_receive_compositor_frame_step.id
    AS viz_receive_compositor_frame_slice_id,
  viz_receive_compositor_frame_step.task_start_time_ts
    AS viz_receive_compositor_frame_task_ts,
  viz_receive_compositor_frame_step.ts AS viz_receive_compositor_frame_ts,
  viz_receive_compositor_frame_step.ts
    + viz_receive_compositor_frame_step.dur
    AS viz_receive_compositor_frame_end_ts,
  viz_receive_compositor_frame_step.utid AS viz_compositor_utid,
  -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
  viz_draw_and_swap_step.id AS viz_draw_and_swap_slice_id,
  viz_draw_and_swap_step.task_start_time_ts
    AS viz_draw_and_swap_task_ts,
  viz_draw_and_swap_step.ts AS viz_draw_and_swap_ts,
  -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
  viz_send_buffer_swap_step.id AS viz_send_buffer_swap_slice_id,
  viz_send_buffer_swap_step.ts + viz_send_buffer_swap_step.dur
    AS viz_send_buffer_swap_end_ts,
  -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
  viz_swap_buffers_step.id AS viz_swap_buffers_slice_id,
  viz_swap_buffers_step.task_start_time_ts AS viz_swap_buffers_task_ts,
  viz_swap_buffers_step.ts AS viz_swap_buffers_ts,
  viz_swap_buffers_step.ts + viz_swap_buffers_step.dur
    AS viz_swap_buffers_end_ts,
  viz_swap_buffers_step.utid AS viz_gpu_thread_utid,
  -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
  chrome_event_latency.buffer_available_timestamp,
  chrome_event_latency.buffer_ready_timestamp,
  chrome_event_latency.latch_timestamp,
  chrome_event_latency.swap_end_timestamp,
  chrome_event_latency.presentation_timestamp
FROM _chrome_scroll_update_refs refs
LEFT JOIN chrome_coalesced_inputs chrome_coalesced_input
  ON chrome_coalesced_input.coalesced_latency_id = refs.scroll_update_latency_id
-- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
LEFT JOIN chrome_event_latencies chrome_event_latency
  ON chrome_event_latency.scroll_update_id = refs.scroll_update_latency_id
-- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
LEFT JOIN chrome_input_pipeline_steps touch_move_received_step
  ON
    refs.touch_move_latency_id = touch_move_received_step.latency_id
    AND touch_move_received_step.step = 'STEP_SEND_INPUT_EVENT_UI'
    AND touch_move_received_step.input_type = 'TOUCH_MOVE_EVENT'
-- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
LEFT JOIN chrome_input_pipeline_steps touch_move_processed_step
  ON
    touch_move_processed_step.latency_id = refs.touch_move_latency_id
    AND touch_move_processed_step.step = 'STEP_TOUCH_EVENT_HANDLED'
    AND touch_move_processed_step.input_type = 'TOUCH_MOVE_EVENT'
-- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
LEFT JOIN chrome_input_pipeline_steps scroll_update_created_step
  ON
    scroll_update_created_step.latency_id = refs.scroll_update_latency_id
    AND scroll_update_created_step.step = 'STEP_SEND_INPUT_EVENT_UI'
    AND scroll_update_created_step.input_type
      = 'GESTURE_SCROLL_UPDATE_EVENT'
-- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
LEFT JOIN chrome_input_pipeline_steps compositor_dispatch_step
  ON
    compositor_dispatch_step.latency_id = refs.scroll_update_latency_id
    AND compositor_dispatch_step.step = 'STEP_HANDLE_INPUT_EVENT_IMPL'
    AND compositor_dispatch_step.input_type
      = 'GESTURE_SCROLL_UPDATE_EVENT'
-- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
LEFT JOIN chrome_input_pipeline_steps compositor_resample_step
  ON
    compositor_resample_step.latency_id = refs.scroll_update_latency_id
    AND compositor_resample_step.step = 'STEP_RESAMPLE_SCROLL_EVENTS'
    AND compositor_resample_step.input_type
      = 'GESTURE_SCROLL_UPDATE_EVENT'
-- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
LEFT JOIN chrome_input_pipeline_steps compositor_coalesced_input_handled_step
  ON
    compositor_coalesced_input_handled_step.latency_id
      = refs.scroll_update_latency_id
    AND compositor_coalesced_input_handled_step.step
      = 'STEP_DID_HANDLE_INPUT_AND_OVERSCROLL'
    AND compositor_coalesced_input_handled_step.input_type
      = 'GESTURE_SCROLL_UPDATE_EVENT'
-- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
LEFT JOIN
  chrome_graphics_pipeline_surface_frame_steps
    compositor_generate_compositor_frame_step
  ON
    compositor_generate_compositor_frame_step.surface_frame_trace_id
      = refs.surface_frame_id
    AND compositor_generate_compositor_frame_step.step
      = 'STEP_GENERATE_COMPOSITOR_FRAME'
-- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
LEFT JOIN
  chrome_graphics_pipeline_surface_frame_steps
    compositor_submit_compositor_frame_step
  ON
    compositor_submit_compositor_frame_step.surface_frame_trace_id
      = refs.surface_frame_id
    AND compositor_submit_compositor_frame_step.step
      = 'STEP_SUBMIT_COMPOSITOR_FRAME'
-- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
LEFT JOIN
  chrome_graphics_pipeline_surface_frame_steps
    viz_receive_compositor_frame_step
  ON
    viz_receive_compositor_frame_step.surface_frame_trace_id
      = refs.surface_frame_id
    AND viz_receive_compositor_frame_step.step
      = 'STEP_RECEIVE_COMPOSITOR_FRAME'
-- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
LEFT JOIN
  chrome_graphics_pipeline_display_frame_steps viz_draw_and_swap_step
  ON
    viz_draw_and_swap_step.display_trace_id = refs.display_trace_id
    AND viz_draw_and_swap_step.step = 'STEP_DRAW_AND_SWAP'
-- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
LEFT JOIN
  chrome_graphics_pipeline_display_frame_steps viz_send_buffer_swap_step
  ON
    viz_send_buffer_swap_step.display_trace_id = refs.display_trace_id
    AND viz_send_buffer_swap_step.step = 'STEP_SEND_BUFFER_SWAP'
-- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
LEFT JOIN chrome_graphics_pipeline_display_frame_steps viz_swap_buffers_step
  ON
    viz_swap_buffers_step.display_trace_id = refs.display_trace_id
    AND viz_swap_buffers_step.step = 'STEP_BUFFER_SWAP_POST_SUBMIT';

-- Intermediate helper table with timestamps and slice ids for the critical path
-- stages during scrolling.
CREATE PERFETTO TABLE _scroll_update_durations_and_metadata
AS
SELECT
  id,
  vsync_interval_ms,
  is_presented,
  is_janky,
  is_inertial,
  coalesced_into IS NOT NULL AS is_coalesced,
  -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
  -- Ids
  browser_utid,
  touch_move_received_slice_id,
  -- Timestamps
  generation_ts,
  touch_move_received_ts,
  -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
  -- Ids
  scroll_update_created_slice_id,
  -- Timestamps
  scroll_update_created_ts,
  scroll_update_created_end_ts,
  -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
  -- Ids
  compositor_utid,
  compositor_dispatch_slice_id,
  -- Timestamps
  COALESCE(compositor_dispatch_task_ts, compositor_dispatch_ts)
    AS compositor_dispatch_ts,
  compositor_dispatch_end_ts,
  -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
  -- Ids
  compositor_resample_slice_id,
  compositor_coalesced_input_handled_slice_id,
  -- Timestamps
  COALESCE(
    compositor_resample_task_ts,
    compositor_resample_ts,
    compositor_coalesced_input_handled_ts) AS compositor_on_begin_frame_ts,
  compositor_coalesced_input_handled_end_ts AS compositor_on_begin_frame_end_ts,
  -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
  -- Ids
  compositor_generate_compositor_frame_slice_id,
  -- Timestamps
  COALESCE(
    compositor_generate_compositor_frame_task_ts,
    compositor_generate_compositor_frame_ts)
    AS compositor_generate_compositor_frame_ts,
  -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
  -- Ids
  compositor_submit_compositor_frame_slice_id,
  -- Timestamps
  compositor_submit_compositor_frame_ts,
  compositor_submit_compositor_frame_end_ts,
  -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
  -- Ids
  viz_compositor_utid,
  viz_receive_compositor_frame_slice_id,
  -- Timestamps
  COALESCE(
    viz_receive_compositor_frame_task_ts, viz_receive_compositor_frame_ts)
    AS viz_receive_compositor_frame_ts,
  viz_receive_compositor_frame_end_ts,
  -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
  -- Ids
  viz_draw_and_swap_slice_id,
  -- Timestamps
  COALESCE(viz_draw_and_swap_task_ts, viz_draw_and_swap_ts)
    AS viz_draw_and_swap_ts,
  -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
  -- Ids
  viz_send_buffer_swap_slice_id,
  -- Timestamps
  viz_send_buffer_swap_end_ts,
  -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
  -- Ids
  viz_gpu_thread_utid,
  viz_swap_buffers_slice_id,
  -- Timestamps
  COALESCE(viz_swap_buffers_task_ts, viz_swap_buffers_ts)
    AS viz_swap_buffers_ts,
  viz_swap_buffers_end_ts,
  -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
  -- Timestamps
  latch_timestamp,
  swap_end_timestamp,
  presentation_timestamp
FROM _scroll_update_timestamps_and_metadata;

-- Defines slices for all of the individual scrolls in a trace based on the
-- LatencyInfo-based scroll definition.
--
-- NOTE: this view of top level scrolls is based on the LatencyInfo definition
-- of a scroll, which differs subtly from the definition based on
-- EventLatencies.
-- TODO(b/278684408): add support for tracking scrolls across multiple Chrome/
-- WebView instances. Currently gesture_scroll_id unique within an instance, but
-- is not unique across multiple instances. Switching to an EventLatency based
-- definition of scrolls should resolve this.
CREATE PERFETTO TABLE chrome_scrolls(
  -- The unique identifier of the scroll.
  id LONG,
  -- The start timestamp of the scroll.
  ts TIMESTAMP,
  -- The duration of the scroll.
  dur DURATION,
  -- The earliest timestamp of the EventLatency slice of the GESTURE_SCROLL_BEGIN type for the
  -- corresponding scroll id.
  gesture_scroll_begin_ts TIMESTAMP,
  -- The earliest timestamp of the EventLatency slice of the GESTURE_SCROLL_END type /
  -- the latest timestamp of the EventLatency slice of the GESTURE_SCROLL_UPDATE type for the
  -- corresponding scroll id.
  gesture_scroll_end_ts TIMESTAMP
) AS
WITH all_scrolls AS (
  SELECT
    event_type AS name,
    ts,
    dur,
    scroll_id
  FROM chrome_gesture_scroll_events
),
scroll_starts AS (
  SELECT
    scroll_id,
    MIN(ts) AS gesture_scroll_begin_ts
  FROM all_scrolls
  WHERE name = 'GESTURE_SCROLL_BEGIN'
  GROUP BY scroll_id
),
scroll_ends AS (
  SELECT
    scroll_id,
    MAX(ts) AS gesture_scroll_end_ts
  FROM all_scrolls
  WHERE name IN (
    'GESTURE_SCROLL_UPDATE',
    'FIRST_GESTURE_SCROLL_UPDATE',
    'INERTIAL_GESTURE_SCROLL_UPDATE',
    'GESTURE_SCROLL_END'
  )
  GROUP BY scroll_id
)
SELECT
  sa.scroll_id AS id,
  MIN(ts) AS ts,
  cast_int!(MAX(ts + dur) - MIN(ts)) AS dur,
  ss.gesture_scroll_begin_ts AS gesture_scroll_begin_ts,
  se.gesture_scroll_end_ts AS gesture_scroll_end_ts
FROM all_scrolls sa
  LEFT JOIN scroll_starts ss ON
    sa.scroll_id = ss.scroll_id
  LEFT JOIN scroll_ends se ON
    sa.scroll_id = se.scroll_id
GROUP BY sa.scroll_id;

-- Timestamps and durations for the critical path stages during scrolling.
CREATE PERFETTO TABLE chrome_scroll_update_info(
  -- Id of the `LatencyInfo.Flow` slices corresponding to this scroll event.
  id INT,
  -- Vsync interval (in milliseconds).
  vsync_interval_ms DOUBLE,
  -- Whether this input event was presented.
  is_presented BOOL,
  -- Whether the corresponding frame is janky. This comes directly from
  -- `perfetto.protos.EventLatency`.
  is_janky BOOL,
  -- Whether the corresponding scroll is inertial (fling).
  -- If this is `true`, "generation" and "touch_move" related timestamps and
  -- durations will be null.
  is_inertial BOOL,
  -- Whether the corresponding input event was coalesced into another.
  is_coalesced BOOL,
  -- Input generation timestamp (from the Android system).
  generation_ts INT,
  -- Duration from input generation to when the browser received the input.
  generation_to_browser_main_dur INT,
  -- Utid for the browser main thread.
  browser_utid INT,
  -- Slice id for the `STEP_SEND_INPUT_EVENT_UI` slice for the touch move.
  touch_move_received_slice_id INT,
  -- Timestamp for the `STEP_SEND_INPUT_EVENT_UI` slice for the touch move.
  touch_move_received_ts INT,
  -- Duration for processing  a `TouchMove` event.
  touch_move_processing_dur INT,
  -- Slice id for the `STEP_SEND_INPUT_EVENT_UI` slice for the gesture scroll.
  scroll_update_created_slice_id INT,
  -- Timestamp for the `STEP_SEND_INPUT_EVENT_UI` slice for the gesture scroll.
  scroll_update_created_ts INT,
  -- Duration for creating a `GestureScrollUpdate` from a `TouchMove` event.
  scroll_update_processing_dur INT,
  -- End timestamp for the `STEP_SEND_INPUT_EVENT_UI` slice for the above.
  scroll_update_created_end_ts INT,
  -- Duration between the browser and compositor dispatch.
  browser_to_compositor_delay_dur INT,
  -- Utid for the renderer compositor thread.
  compositor_utid INT,
  -- Slice id for the `STEP_HANDLE_INPUT_EVENT_IMPL` slice.
  compositor_dispatch_slice_id INT,
  -- Timestamp for the `STEP_HANDLE_INPUT_EVENT_IMPL` slice or the
  -- containing task (if available).
  compositor_dispatch_ts INT,
  -- Duration for the compositor dispatch itself.
  compositor_dispatch_dur INT,
  -- End timestamp for the `STEP_HANDLE_INPUT_EVENT_IMPL` slice.
  compositor_dispatch_end_ts INT,
  -- Duration between compositor dispatch and input resampling work.
  compositor_dispatch_to_on_begin_frame_delay_dur INT,
  -- Slice id for the `STEP_RESAMPLE_SCROLL_EVENTS` slice.
  compositor_resample_slice_id INT,
  -- Slice id for the `STEP_DID_HANDLE_INPUT_AND_OVERSCROLL` slice.
  compositor_coalesced_input_handled_slice_id INT,
  -- Start timestamp for work done on the input during "OnBeginFrame".
  compositor_on_begin_frame_ts INT,
  -- Duration of the "OnBeginFrame" work for this input.
  compositor_on_begin_frame_dur INT,
  -- End timestamp for work done on the input during "OnBeginFrame".
  compositor_on_begin_frame_end_ts INT,
  -- Delay until the compositor work for generating the frame begins.
  compositor_on_begin_frame_to_generation_delay_dur INT,
  -- Slice id for the `STEP_GENERATE_COMPOSITOR_FRAME` slice.
  compositor_generate_compositor_frame_slice_id INT,
  -- Timestamp for the `STEP_GENERATE_COMPOSITOR_FRAME` slice or the
  -- containing task (if available).
  compositor_generate_compositor_frame_ts INT,
  -- Duration between generating and submitting the compositor frame.
  compositor_generate_frame_to_submit_frame_dur INT,
  -- Slice id for the `STEP_SUBMIT_COMPOSITOR_FRAME` slice.
  compositor_submit_compositor_frame_slice_id INT,
  -- Timestamp for the `STEP_SUBMIT_COMPOSITOR_FRAME` slice.
  compositor_submit_compositor_frame_ts INT,
  -- Duration for submitting the compositor frame (to viz).
  compositor_submit_frame_dur INT,
  -- End timestamp for the `STEP_SUBMIT_COMPOSITOR_FRAME` slice.
  compositor_submit_compositor_frame_end_ts INT,
  -- Delay when a compositor frame is sent from the renderer to viz.
  compositor_to_viz_delay_dur INT,
  -- Utid for the viz compositor thread.
  viz_compositor_utid INT,
  -- Slice id for the `STEP_RECEIVE_COMPOSITOR_FRAME` slice.
  viz_receive_compositor_frame_slice_id INT,
  -- Timestamp for the `STEP_RECEIVE_COMPOSITOR_FRAME` slice or the
  -- containing task (if available).
  viz_receive_compositor_frame_ts INT,
  -- Duration of the viz work done on receiving the compositor frame.
  viz_receive_compositor_frame_dur INT,
  -- End timestamp for the `STEP_RECEIVE_COMPOSITOR_FRAME` slice.
  viz_receive_compositor_frame_end_ts INT,
  -- Duration between viz receiving the compositor frame to frame draw.
  viz_wait_for_draw_dur INT,
  -- Slice id for the `STEP_DRAW_AND_SWAP` slice.
  viz_draw_and_swap_slice_id INT,
  -- Timestamp for the `STEP_DRAW_AND_SWAP` slice or the
  -- containing task (if available).
  viz_draw_and_swap_ts INT,
  -- Duration for the viz drawing/swapping work for this frame.
  viz_draw_and_swap_dur INT,
  -- Slice id for the `STEP_SEND_BUFFER_SWAP` slice.
  viz_send_buffer_swap_slice_id INT,
  -- End timestamp for the `STEP_SEND_BUFFER_SWAP` slice.
  viz_send_buffer_swap_end_ts INT,
  -- Delay between viz work on compositor thread and `CompositorGpuThread`.
  viz_to_gpu_delay_dur INT,
  -- Utid for the viz `CompositorGpuThread`.
  viz_gpu_thread_utid INT,
  -- Slice id for the `STEP_BUFFER_SWAP_POST_SUBMIT` slice.
  viz_swap_buffers_slice_id INT,
  -- Timestamp for the `STEP_BUFFER_SWAP_POST_SUBMIT` slice or the
  -- containing task (if available).
  viz_swap_buffers_ts INT,
  -- Duration of frame buffer swapping work on viz.
  viz_swap_buffers_dur INT,
  -- End timestamp for the `STEP_BUFFER_SWAP_POST_SUBMIT` slice.
  viz_swap_buffers_end_ts INT,
  -- Duration of `EventLatency`'s `BufferReadyToLatch` step.
  viz_swap_buffers_to_latch_dur INT,
  -- Timestamp for `EventLatency`'s `LatchToSwapEnd` step.
  latch_timestamp INT,
  -- Duration of `EventLatency`'s `LatchToSwapEnd` step.
  viz_latch_to_swap_end_dur INT,
  -- Timestamp for `EventLatency`'s `SwapEndToPresentationCompositorFrame` step.
  swap_end_timestamp INT,
  -- Duration of `EventLatency`'s `SwapEndToPresentationCompositorFrame` step.
  swap_end_to_presentation_dur INT,
  -- Presentation timestamp for the frame.
  presentation_timestamp INT)
AS
SELECT
  id,
  -- TODO(b:380868337): This is sometimes unexpectedly 0; check/fix this.
  vsync_interval_ms,
  is_presented,
  is_janky,
  is_inertial,
  -- TODO(b:380868337): Check/fix this for flings.
  is_coalesced,
  -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
  -- No applicable utid (duration between two threads).
  -- No applicable slice id (duration between two threads).
  generation_ts,
  touch_move_received_ts - generation_ts AS generation_to_browser_main_dur,
  -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
  browser_utid,
  touch_move_received_slice_id,
  touch_move_received_ts,
  scroll_update_created_ts - touch_move_received_ts
    AS touch_move_processing_dur,
  -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
  -- On `browser_utid`.
  scroll_update_created_slice_id,
  scroll_update_created_ts,
  scroll_update_created_end_ts - scroll_update_created_ts
    AS scroll_update_processing_dur,
  -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
  -- No applicable utid (duration between two threads).
  -- No applicable slice id (duration between two threads).
  scroll_update_created_end_ts,
  -- TODO(b:380868337): This is sometimes negative; check/fix this.
  compositor_dispatch_ts - scroll_update_created_end_ts
    AS browser_to_compositor_delay_dur,
  -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
  compositor_utid,
  compositor_dispatch_slice_id,
  compositor_dispatch_ts,
  compositor_dispatch_end_ts - compositor_dispatch_ts
    AS compositor_dispatch_dur,
  -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
  -- On `compositor_utid`.
  -- No applicable slice id (duration between two slices).
  compositor_dispatch_end_ts,
  -- TODO(b:380868337): This is sometimes negative; check/fix this.
  compositor_on_begin_frame_ts - compositor_dispatch_end_ts
    AS compositor_dispatch_to_on_begin_frame_delay_dur,
  -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
  -- On `compositor_utid`.
  -- `compositor_on_begin_frame_dur` can depend on two slices.
  compositor_resample_slice_id,
  compositor_coalesced_input_handled_slice_id,
  compositor_on_begin_frame_ts,
  compositor_on_begin_frame_end_ts - compositor_on_begin_frame_ts
    AS compositor_on_begin_frame_dur,
  -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
  -- On `compositor_utid`.
  -- No applicable slice id (duration between two slices).
  compositor_on_begin_frame_end_ts,
  compositor_generate_compositor_frame_ts - compositor_on_begin_frame_end_ts
    AS compositor_on_begin_frame_to_generation_delay_dur,
  -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
  -- On `compositor_utid`.
  compositor_generate_compositor_frame_slice_id,
  -- TODO(b:380868337): This is sometimes unexpectedly null; check/fix this.
  compositor_generate_compositor_frame_ts,
  compositor_submit_compositor_frame_ts
    - compositor_generate_compositor_frame_ts
    AS compositor_generate_frame_to_submit_frame_dur,
  -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
  -- On `compositor_utid`.
  compositor_submit_compositor_frame_slice_id,
  compositor_submit_compositor_frame_ts,
  compositor_submit_compositor_frame_end_ts
    - compositor_submit_compositor_frame_ts AS compositor_submit_frame_dur,
  -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
  -- No applicable utid (duration between two threads).
  -- No applicable slice id (duration between two threads).
  compositor_submit_compositor_frame_end_ts,
  -- TODO(b:380868337): This is sometimes negative; check/fix this.
  viz_receive_compositor_frame_ts - compositor_submit_compositor_frame_end_ts
    AS compositor_to_viz_delay_dur,
  -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
  viz_compositor_utid,
  viz_receive_compositor_frame_slice_id,
  viz_receive_compositor_frame_ts,
  viz_receive_compositor_frame_end_ts - viz_receive_compositor_frame_ts
    AS viz_receive_compositor_frame_dur,
  -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
  -- On `viz_compositor_utid`.
  -- No applicable slice id (duration between two slices).
  viz_receive_compositor_frame_end_ts,
  viz_draw_and_swap_ts - viz_receive_compositor_frame_end_ts
    AS viz_wait_for_draw_dur,
  -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
  -- On `viz_compositor_utid`.
  viz_draw_and_swap_slice_id,
  viz_draw_and_swap_ts,
  viz_send_buffer_swap_end_ts - viz_draw_and_swap_ts AS viz_draw_and_swap_dur,
  -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
  -- No applicable utid (duration between two threads).
  viz_send_buffer_swap_slice_id,
  viz_send_buffer_swap_end_ts,
  viz_swap_buffers_ts - viz_send_buffer_swap_end_ts AS viz_to_gpu_delay_dur,
  -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
  viz_gpu_thread_utid,
  viz_swap_buffers_slice_id,
  viz_swap_buffers_ts,
  viz_swap_buffers_end_ts - viz_swap_buffers_ts AS viz_swap_buffers_dur,
  -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
  viz_swap_buffers_end_ts,
  latch_timestamp - viz_swap_buffers_end_ts AS viz_swap_buffers_to_latch_dur,
  -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
  latch_timestamp,
  swap_end_timestamp - latch_timestamp AS viz_latch_to_swap_end_dur,
  -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
  swap_end_timestamp,
  presentation_timestamp - swap_end_timestamp AS swap_end_to_presentation_dur,
  -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
  presentation_timestamp
FROM _scroll_update_durations_and_metadata;
