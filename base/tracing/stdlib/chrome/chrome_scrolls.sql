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
  scroll_update_latency_id LONG,
  -- Id of the touch move input corresponding to this scroll update.
  touch_move_latency_id LONG,
  -- Id of the `EventLatency` of the frame that the input was presented in.
  presentation_latency_id LONG,
  -- Id of the frame pipeline (`Graphics.Pipeline`), pre-surface aggregation.
  surface_frame_id LONG,
  -- Id of the frame pipeline (`Graphics.Pipeline`), post-surface aggregation.
  display_trace_id LONG)
AS
SELECT
  scroll_update.latency_id AS scroll_update_latency_id,
  chrome_touch_move_to_scroll_update.touch_move_latency_id,
  COALESCE(
    chrome_coalesced_inputs.presented_latency_id,
    scroll_update.latency_id
  ) AS presentation_latency_id,
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
LEFT JOIN chrome_coalesced_inputs
  ON chrome_coalesced_inputs.coalesced_latency_id = scroll_update.latency_id
WHERE scroll_update.input_type = 'GESTURE_SCROLL_UPDATE_EVENT';

-- Timestamps and other related information for events during the
-- input-associated (before inputs are coalesced into a frame) stages of a
-- scroll.
CREATE PERFETTO TABLE _scroll_update_input_timestamps_and_metadata
AS
SELECT
  refs.scroll_update_latency_id AS id,
  refs.presentation_latency_id AS presented_in_frame_id,
  -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
  chrome_event_latency.is_presented AS is_presented,
  chrome_event_latency.is_janky_scrolled_frame AS is_janky,
  chrome_event_latency.event_type
    = 'INERTIAL_GESTURE_SCROLL_UPDATE' AS is_inertial,
  chrome_event_latency.event_type
    = 'FIRST_GESTURE_SCROLL_UPDATE' AS is_first_scroll_update_in_scroll,
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
  compositor_coalesced_input_handled_step.slice_id
    AS compositor_coalesced_input_handled_slice_id,
  compositor_coalesced_input_handled_step.ts
    AS compositor_coalesced_input_handled_ts,
  compositor_coalesced_input_handled_step.ts
    + compositor_coalesced_input_handled_step.dur
    AS compositor_coalesced_input_handled_end_ts
FROM _chrome_scroll_update_refs refs
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
LEFT JOIN chrome_input_pipeline_steps compositor_coalesced_input_handled_step
  ON
    compositor_coalesced_input_handled_step.latency_id
      = refs.scroll_update_latency_id
    AND compositor_coalesced_input_handled_step.step
      = 'STEP_DID_HANDLE_INPUT_AND_OVERSCROLL'
    AND compositor_coalesced_input_handled_step.input_type
      = 'GESTURE_SCROLL_UPDATE_EVENT';

-- Timestamps and durations for the input-associated (before coalescing inputs
-- into a frame) stages of a scroll.
CREATE PERFETTO TABLE chrome_scroll_update_input_info(
  -- Id of the `LatencyInfo.Flow` slices corresponding to this scroll event.
  id LONG,
  -- Id of the frame that this input was presented in. Can be joined with
  -- `chrome_scroll_update_frame_info.id`.
  presented_in_frame_id LONG,
  -- Whether this input event was presented.
  is_presented BOOL,
  -- Whether the corresponding frame is janky. This comes directly from
  -- `perfetto.protos.EventLatency`.
  is_janky BOOL,
  -- Whether the corresponding scroll is inertial (fling).
  -- If this is `true`, "generation" and "touch_move" related timestamps and
  -- durations will be null.
  is_inertial BOOL,
  -- Whether this is the first update in a scroll.
  -- First scroll update can never be janky.
  is_first_scroll_update_in_scroll BOOL,
  -- Whether this is the first input that was presented in frame
  -- `presented_in_frame_id`.
  is_first_scroll_update_in_frame BOOL,
  -- Input generation timestamp (from the Android system).
  generation_ts TIMESTAMP,
  -- Duration from input generation to when the browser received the input.
  generation_to_browser_main_dur DURATION,
  -- Utid for the browser main thread.
  browser_utid LONG,
  -- Slice id for the `STEP_SEND_INPUT_EVENT_UI` slice for the touch move.
  touch_move_received_slice_id LONG,
  -- Timestamp for the `STEP_SEND_INPUT_EVENT_UI` slice for the touch move.
  touch_move_received_ts TIMESTAMP,
  -- Duration for processing  a `TouchMove` event.
  touch_move_processing_dur DURATION,
  -- Slice id for the `STEP_SEND_INPUT_EVENT_UI` slice for the gesture scroll.
  scroll_update_created_slice_id LONG,
  -- Timestamp for the `STEP_SEND_INPUT_EVENT_UI` slice for the gesture scroll.
  scroll_update_created_ts TIMESTAMP,
  -- Duration for creating a `GestureScrollUpdate` from a `TouchMove` event.
  scroll_update_processing_dur DURATION,
  -- End timestamp for the `STEP_SEND_INPUT_EVENT_UI` slice for the above.
  scroll_update_created_end_ts TIMESTAMP,
  -- Duration between the browser and compositor dispatch.
  browser_to_compositor_delay_dur DURATION,
  -- Utid for the renderer compositor thread.
  compositor_utid LONG,
  -- Slice id for the `STEP_HANDLE_INPUT_EVENT_IMPL` slice.
  compositor_dispatch_slice_id LONG,
  -- Timestamp for the `STEP_HANDLE_INPUT_EVENT_IMPL` slice or the
  -- containing task (if available).
  compositor_dispatch_ts TIMESTAMP,
  -- Duration for the compositor dispatch itself.
  compositor_dispatch_dur DURATION,
  -- End timestamp for the `STEP_HANDLE_INPUT_EVENT_IMPL` slice.
  compositor_dispatch_end_ts TIMESTAMP,
  -- Duration between compositor dispatch and coalescing input.
  compositor_dispatch_to_coalesced_input_handled_dur DURATION,
  -- Slice id for the `STEP_DID_HANDLE_INPUT_AND_OVERSCROLL` slice.
  compositor_coalesced_input_handled_slice_id LONG,
  -- Timestamp for the `STEP_DID_HANDLE_INPUT_AND_OVERSCROLL` slice.
  compositor_coalesced_input_handled_ts TIMESTAMP,
  -- Duration for the `STEP_DID_HANDLE_INPUT_AND_OVERSCROLL` slice.
  compositor_coalesced_input_handled_dur DURATION,
  -- End timestamp for the `STEP_DID_HANDLE_INPUT_AND_OVERSCROLL` slice.
  compositor_coalesced_input_handled_end_ts TIMESTAMP
) AS
WITH
processed_timestamps_and_metadata AS (
  SELECT
    id,
    presented_in_frame_id,
    is_presented,
    is_janky,
    is_inertial,
    is_first_scroll_update_in_scroll,
    ROW_NUMBER()
      OVER (PARTITION BY presented_in_frame_id ORDER BY generation_ts ASC) = 1
      AS is_first_scroll_update_in_frame,
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
    compositor_coalesced_input_handled_slice_id,
    -- Timestamps
    compositor_coalesced_input_handled_ts,
    compositor_coalesced_input_handled_end_ts
  FROM _scroll_update_input_timestamps_and_metadata
)
SELECT
  id,
  presented_in_frame_id,
  is_presented,
  is_janky,
  is_inertial,
  is_first_scroll_update_in_scroll,
  is_first_scroll_update_in_frame,
  -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
  -- No applicable utid (duration between two threads).
  -- No applicable slice id (duration between two threads).
  generation_ts,
  -- Flings don't have a touch move event so make GenerationToBrowserMain span
  -- all the way to the creation of the gesture scroll update.
  IIF(
    is_inertial AND touch_move_received_ts IS NULL,
    scroll_update_created_ts,
    touch_move_received_ts
  ) - generation_ts AS generation_to_browser_main_dur,
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
  compositor_coalesced_input_handled_ts - compositor_dispatch_end_ts
    AS compositor_dispatch_to_coalesced_input_handled_dur,
  -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
  -- On `compositor_utid`.
  compositor_coalesced_input_handled_slice_id,
  compositor_coalesced_input_handled_ts,
  compositor_coalesced_input_handled_end_ts
    - compositor_coalesced_input_handled_ts
    AS compositor_coalesced_input_handled_dur,
  -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
  compositor_coalesced_input_handled_end_ts
FROM processed_timestamps_and_metadata;

-- Timestamps and other related information for events during the
-- frame-associated (after inputs are coalesced into a frame) stages of a
-- scroll.
CREATE PERFETTO TABLE _scroll_update_frame_timestamps_and_metadata
AS
SELECT
  refs.scroll_update_latency_id AS id,
  -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
  chrome_event_latency.vsync_interval_ms AS vsync_interval_ms,
  -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
  compositor_resample_step.slice_id AS compositor_resample_slice_id,
  compositor_resample_step.task_start_time_ts
    AS compositor_resample_task_ts,
  compositor_resample_step.ts AS compositor_resample_ts,
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
LEFT JOIN chrome_event_latencies chrome_event_latency
  ON chrome_event_latency.scroll_update_id = refs.presentation_latency_id
-- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
LEFT JOIN chrome_input_pipeline_steps compositor_resample_step
  ON
    compositor_resample_step.latency_id = refs.presentation_latency_id
    AND compositor_resample_step.step = 'STEP_RESAMPLE_SCROLL_EVENTS'
    AND compositor_resample_step.input_type
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
    AND viz_swap_buffers_step.step = 'STEP_BUFFER_SWAP_POST_SUBMIT'
-- Filter out inputs which were coalesced into a different frame (so that rows
-- of this table correspond to frames).
WHERE refs.scroll_update_latency_id = refs.presentation_latency_id;

-- Timestamps and durations for the frame-associated (after coalescing inputs
-- into a frame) stages of a scroll.
CREATE PERFETTO TABLE chrome_scroll_update_frame_info(
  -- Id of the `LatencyInfo.Flow` slices corresponding to this scroll event.
  id LONG,
  -- Vsync interval (in milliseconds).
  vsync_interval_ms DOUBLE,
  -- Slice id for the `STEP_RESAMPLE_SCROLL_EVENTS` slice.
  compositor_resample_slice_id LONG,
  -- Timestamp for the `STEP_RESAMPLE_SCROLL_EVENTS` slice.
  compositor_resample_ts TIMESTAMP,
  -- Slice id for the `STEP_GENERATE_COMPOSITOR_FRAME` slice.
  compositor_generate_compositor_frame_slice_id LONG,
  -- Timestamp for the `STEP_GENERATE_COMPOSITOR_FRAME` slice or the
  -- containing task (if available).
  compositor_generate_compositor_frame_ts TIMESTAMP,
  -- Duration between generating and submitting the compositor frame.
  compositor_generate_frame_to_submit_frame_dur DURATION,
  -- Slice id for the `STEP_SUBMIT_COMPOSITOR_FRAME` slice.
  compositor_submit_compositor_frame_slice_id LONG,
  -- Timestamp for the `STEP_SUBMIT_COMPOSITOR_FRAME` slice.
  compositor_submit_compositor_frame_ts TIMESTAMP,
  -- Duration for submitting the compositor frame (to viz).
  compositor_submit_frame_dur DURATION,
  -- End timestamp for the `STEP_SUBMIT_COMPOSITOR_FRAME` slice.
  compositor_submit_compositor_frame_end_ts TIMESTAMP,
  -- Delay when a compositor frame is sent from the renderer to viz.
  compositor_to_viz_delay_dur DURATION,
  -- Utid for the viz compositor thread.
  viz_compositor_utid LONG,
  -- Slice id for the `STEP_RECEIVE_COMPOSITOR_FRAME` slice.
  viz_receive_compositor_frame_slice_id LONG,
  -- Timestamp for the `STEP_RECEIVE_COMPOSITOR_FRAME` slice or the
  -- containing task (if available).
  viz_receive_compositor_frame_ts TIMESTAMP,
  -- Duration of the viz work done on receiving the compositor frame.
  viz_receive_compositor_frame_dur DURATION,
  -- End timestamp for the `STEP_RECEIVE_COMPOSITOR_FRAME` slice.
  viz_receive_compositor_frame_end_ts TIMESTAMP,
  -- Duration between viz receiving the compositor frame to frame draw.
  viz_wait_for_draw_dur DURATION,
  -- Slice id for the `STEP_DRAW_AND_SWAP` slice.
  viz_draw_and_swap_slice_id LONG,
  -- Timestamp for the `STEP_DRAW_AND_SWAP` slice or the
  -- containing task (if available).
  viz_draw_and_swap_ts TIMESTAMP,
  -- Duration for the viz drawing/swapping work for this frame.
  viz_draw_and_swap_dur DURATION,
  -- Slice id for the `STEP_SEND_BUFFER_SWAP` slice.
  viz_send_buffer_swap_slice_id LONG,
  -- End timestamp for the `STEP_SEND_BUFFER_SWAP` slice.
  viz_send_buffer_swap_end_ts TIMESTAMP,
  -- Delay between viz work on compositor thread and `CompositorGpuThread`.
  viz_to_gpu_delay_dur DURATION,
  -- Utid for the viz `CompositorGpuThread`.
  viz_gpu_thread_utid LONG,
  -- Slice id for the `STEP_BUFFER_SWAP_POST_SUBMIT` slice.
  viz_swap_buffers_slice_id LONG,
  -- Timestamp for the `STEP_BUFFER_SWAP_POST_SUBMIT` slice or the
  -- containing task (if available).
  viz_swap_buffers_ts TIMESTAMP,
  -- Duration of frame buffer swapping work on viz.
  viz_swap_buffers_dur DURATION,
  -- End timestamp for the `STEP_BUFFER_SWAP_POST_SUBMIT` slice.
  viz_swap_buffers_end_ts TIMESTAMP,
  -- Duration of `EventLatency`'s `BufferReadyToLatch` step.
  viz_swap_buffers_to_latch_dur DURATION,
  -- Timestamp for `EventLatency`'s `LatchToSwapEnd` step.
  latch_timestamp TIMESTAMP,
  -- Duration of `EventLatency`'s `LatchToSwapEnd` step.
  viz_latch_to_swap_end_dur DURATION,
  -- Timestamp for `EventLatency`'s `SwapEndToPresentationCompositorFrame` step.
  swap_end_timestamp TIMESTAMP,
  -- Duration of `EventLatency`'s `SwapEndToPresentationCompositorFrame` step.
  swap_end_to_presentation_dur DURATION,
  -- Presentation timestamp for the frame.
  presentation_timestamp TIMESTAMP
) AS
WITH
processed_timestamps_and_metadata AS (
  SELECT
    id,
    vsync_interval_ms,
    -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
    -- Ids
    compositor_resample_slice_id,
    -- Timestamps
    COALESCE(
      compositor_resample_task_ts,
      compositor_resample_ts) AS compositor_resample_ts,
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
  FROM _scroll_update_frame_timestamps_and_metadata
)
SELECT
  id,
  -- TODO(b:381062412): This is sometimes unexpectedly 0; check/fix this.
  vsync_interval_ms,
  -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
  -- On `compositor_utid`.
  compositor_resample_slice_id,
  compositor_resample_ts,
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
FROM processed_timestamps_and_metadata;

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
-- This table covers both the input-associated (before coalescing inputs into a
-- frame) and frame-associated (after coalescing inputs into a frame) stages of
-- a scroll:
--
--                              ...
--                               |
--                +--------------+--------------+
--                |                             |
--                V                             V
--   +-------------------------+   +-------------------------+
--   | _scroll_update_INPUT_   |   | _scroll_update_FRAME_   |
--   | timestamps_and_metadata |   | timestamps_and_metadata |
--   +------------+------------+   +------------+------------+
--                |                             |
--                V                             V
--    +-----------------------+     +-----------------------+
--    | chrome_scroll_update_ |     | chrome_scroll_update_ |
--    |       INPUT_info      |     |       FRAME_info      |
--    +-----------+-----------+     +-----------+-----------+
--                |                             |
--                +--------------+--------------+
--                               |
--                               V
--                 +---------------------------+
--                 | chrome_scroll_update_info |
--                 +---------------------------+
CREATE PERFETTO TABLE chrome_scroll_update_info(
  -- Id of the `LatencyInfo.Flow` slices corresponding to this scroll event.
  id LONG,
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
  -- Whether this is the first update in a scroll.
  -- First scroll update can never be janky.
  is_first_scroll_update_in_scroll BOOL,
  -- Whether this is the first input that was presented in the frame.
  is_first_scroll_update_in_frame BOOL,
  -- Input generation timestamp (from the Android system).
  generation_ts TIMESTAMP,
  -- Duration from input generation to when the browser received the input.
  generation_to_browser_main_dur DURATION,
  -- Utid for the browser main thread.
  browser_utid LONG,
  -- Slice id for the `STEP_SEND_INPUT_EVENT_UI` slice for the touch move.
  touch_move_received_slice_id LONG,
  -- Timestamp for the `STEP_SEND_INPUT_EVENT_UI` slice for the touch move.
  touch_move_received_ts TIMESTAMP,
  -- Duration for processing  a `TouchMove` event.
  touch_move_processing_dur DURATION,
  -- Slice id for the `STEP_SEND_INPUT_EVENT_UI` slice for the gesture scroll.
  scroll_update_created_slice_id LONG,
  -- Timestamp for the `STEP_SEND_INPUT_EVENT_UI` slice for the gesture scroll.
  scroll_update_created_ts TIMESTAMP,
  -- Duration for creating a `GestureScrollUpdate` from a `TouchMove` event.
  scroll_update_processing_dur DURATION,
  -- End timestamp for the `STEP_SEND_INPUT_EVENT_UI` slice for the above.
  scroll_update_created_end_ts TIMESTAMP,
  -- Duration between the browser and compositor dispatch.
  browser_to_compositor_delay_dur DURATION,
  -- Utid for the renderer compositor thread.
  compositor_utid LONG,
  -- Slice id for the `STEP_HANDLE_INPUT_EVENT_IMPL` slice.
  compositor_dispatch_slice_id LONG,
  -- Timestamp for the `STEP_HANDLE_INPUT_EVENT_IMPL` slice or the
  -- containing task (if available).
  compositor_dispatch_ts TIMESTAMP,
  -- Duration for the compositor dispatch itself.
  compositor_dispatch_dur DURATION,
  -- End timestamp for the `STEP_HANDLE_INPUT_EVENT_IMPL` slice.
  compositor_dispatch_end_ts TIMESTAMP,
  -- Duration between compositor dispatch and input resampling work.
  compositor_dispatch_to_on_begin_frame_delay_dur DURATION,
  -- Slice id for the `STEP_RESAMPLE_SCROLL_EVENTS` slice.
  compositor_resample_slice_id LONG,
  -- Slice id for the `STEP_DID_HANDLE_INPUT_AND_OVERSCROLL` slice.
  compositor_coalesced_input_handled_slice_id LONG,
  -- Start timestamp for work done on the input during "OnBeginFrame".
  compositor_on_begin_frame_ts TIMESTAMP,
  -- Duration of the "OnBeginFrame" work for this input.
  compositor_on_begin_frame_dur DURATION,
  -- End timestamp for work done on the input during "OnBeginFrame".
  compositor_on_begin_frame_end_ts TIMESTAMP,
  -- Delay until the compositor work for generating the frame begins.
  compositor_on_begin_frame_to_generation_delay_dur DURATION,
  -- Slice id for the `STEP_GENERATE_COMPOSITOR_FRAME` slice.
  compositor_generate_compositor_frame_slice_id LONG,
  -- Timestamp for the `STEP_GENERATE_COMPOSITOR_FRAME` slice or the
  -- containing task (if available).
  compositor_generate_compositor_frame_ts TIMESTAMP,
  -- Duration between generating and submitting the compositor frame.
  compositor_generate_frame_to_submit_frame_dur DURATION,
  -- Slice id for the `STEP_SUBMIT_COMPOSITOR_FRAME` slice.
  compositor_submit_compositor_frame_slice_id LONG,
  -- Timestamp for the `STEP_SUBMIT_COMPOSITOR_FRAME` slice.
  compositor_submit_compositor_frame_ts TIMESTAMP,
  -- Duration for submitting the compositor frame (to viz).
  compositor_submit_frame_dur DURATION,
  -- End timestamp for the `STEP_SUBMIT_COMPOSITOR_FRAME` slice.
  compositor_submit_compositor_frame_end_ts TIMESTAMP,
  -- Delay when a compositor frame is sent from the renderer to viz.
  compositor_to_viz_delay_dur DURATION,
  -- Utid for the viz compositor thread.
  viz_compositor_utid LONG,
  -- Slice id for the `STEP_RECEIVE_COMPOSITOR_FRAME` slice.
  viz_receive_compositor_frame_slice_id LONG,
  -- Timestamp for the `STEP_RECEIVE_COMPOSITOR_FRAME` slice or the
  -- containing task (if available).
  viz_receive_compositor_frame_ts TIMESTAMP,
  -- Duration of the viz work done on receiving the compositor frame.
  viz_receive_compositor_frame_dur DURATION,
  -- End timestamp for the `STEP_RECEIVE_COMPOSITOR_FRAME` slice.
  viz_receive_compositor_frame_end_ts TIMESTAMP,
  -- Duration between viz receiving the compositor frame to frame draw.
  viz_wait_for_draw_dur DURATION,
  -- Slice id for the `STEP_DRAW_AND_SWAP` slice.
  viz_draw_and_swap_slice_id LONG,
  -- Timestamp for the `STEP_DRAW_AND_SWAP` slice or the
  -- containing task (if available).
  viz_draw_and_swap_ts TIMESTAMP,
  -- Duration for the viz drawing/swapping work for this frame.
  viz_draw_and_swap_dur DURATION,
  -- Slice id for the `STEP_SEND_BUFFER_SWAP` slice.
  viz_send_buffer_swap_slice_id LONG,
  -- End timestamp for the `STEP_SEND_BUFFER_SWAP` slice.
  viz_send_buffer_swap_end_ts TIMESTAMP,
  -- Delay between viz work on compositor thread and `CompositorGpuThread`.
  viz_to_gpu_delay_dur DURATION,
  -- Utid for the viz `CompositorGpuThread`.
  viz_gpu_thread_utid LONG,
  -- Slice id for the `STEP_BUFFER_SWAP_POST_SUBMIT` slice.
  viz_swap_buffers_slice_id LONG,
  -- Timestamp for the `STEP_BUFFER_SWAP_POST_SUBMIT` slice or the
  -- containing task (if available).
  viz_swap_buffers_ts TIMESTAMP,
  -- Duration of frame buffer swapping work on viz.
  viz_swap_buffers_dur DURATION,
  -- End timestamp for the `STEP_BUFFER_SWAP_POST_SUBMIT` slice.
  viz_swap_buffers_end_ts TIMESTAMP,
  -- Duration of `EventLatency`'s `BufferReadyToLatch` step.
  viz_swap_buffers_to_latch_dur DURATION,
  -- Timestamp for `EventLatency`'s `LatchToSwapEnd` step.
  latch_timestamp TIMESTAMP,
  -- Duration of `EventLatency`'s `LatchToSwapEnd` step.
  viz_latch_to_swap_end_dur DURATION,
  -- Timestamp for `EventLatency`'s `SwapEndToPresentationCompositorFrame` step.
  swap_end_timestamp TIMESTAMP,
  -- Duration of `EventLatency`'s `SwapEndToPresentationCompositorFrame` step.
  swap_end_to_presentation_dur DURATION,
  -- Presentation timestamp for the frame.
  presentation_timestamp TIMESTAMP)
AS
SELECT
  input.id,
  -- TODO(b:381062412): This is sometimes unexpectedly 0; check/fix this.
  frame.vsync_interval_ms,
  input.is_presented,
  input.is_janky,
  input.is_inertial,
  input.is_first_scroll_update_in_scroll,
  input.is_first_scroll_update_in_frame,
  -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
  -- No applicable utid (duration between two threads).
  -- No applicable slice id (duration between two threads).
  input.generation_ts,
  input.generation_to_browser_main_dur,
  -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
  input.browser_utid,
  input.touch_move_received_slice_id,
  input.touch_move_received_ts,
  input.touch_move_processing_dur,
  -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
  -- On `browser_utid`.
  input.scroll_update_created_slice_id,
  input.scroll_update_created_ts,
  input.scroll_update_processing_dur,
  -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
  -- No applicable utid (duration between two threads).
  -- No applicable slice id (duration between two threads).
  input.scroll_update_created_end_ts,
  -- TODO(b:380868337): This is sometimes negative; check/fix this.
  input.browser_to_compositor_delay_dur,
  -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
  input.compositor_utid,
  input.compositor_dispatch_slice_id,
  input.compositor_dispatch_ts,
  input.compositor_dispatch_dur,
  -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
  -- On `compositor_utid`.
  -- No applicable slice id (duration between two slices).
  input.compositor_dispatch_end_ts,
  -- TODO(b:380868337): This is sometimes negative; check/fix this.
  COALESCE(
    frame.compositor_resample_ts,
    input.compositor_coalesced_input_handled_ts
  ) - input.compositor_dispatch_end_ts
    AS compositor_dispatch_to_on_begin_frame_delay_dur,
  -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
  -- On `compositor_utid`.
  -- `compositor_on_begin_frame_dur` can depend on two slices.
  frame.compositor_resample_slice_id,
  input.compositor_coalesced_input_handled_slice_id,
  COALESCE(
    frame.compositor_resample_ts,
    input.compositor_coalesced_input_handled_ts
  ) AS compositor_on_begin_frame_ts,
  input.compositor_coalesced_input_handled_end_ts - COALESCE(
    frame.compositor_resample_ts,
    input.compositor_coalesced_input_handled_ts
  ) AS compositor_on_begin_frame_dur,
  -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
  -- On `compositor_utid`.
  -- No applicable slice id (duration between two slices).
  input.compositor_coalesced_input_handled_end_ts AS compositor_on_begin_frame_end_ts,
  frame.compositor_generate_compositor_frame_ts - input.compositor_coalesced_input_handled_end_ts
    AS compositor_on_begin_frame_to_generation_delay_dur,
  -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
  -- On `compositor_utid`.
  frame.compositor_generate_compositor_frame_slice_id,
  -- TODO(b:380868337): This is sometimes unexpectedly null; check/fix this.
  frame.compositor_generate_compositor_frame_ts,
  frame.compositor_generate_frame_to_submit_frame_dur,
  -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
  -- On `compositor_utid`.
  frame.compositor_submit_compositor_frame_slice_id,
  frame.compositor_submit_compositor_frame_ts,
  frame.compositor_submit_frame_dur,
  -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
  -- No applicable utid (duration between two threads).
  -- No applicable slice id (duration between two threads).
  frame.compositor_submit_compositor_frame_end_ts,
  -- TODO(b:380868337): This is sometimes negative; check/fix this.
  frame.compositor_to_viz_delay_dur,
  -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
  frame.viz_compositor_utid,
  frame.viz_receive_compositor_frame_slice_id,
  frame.viz_receive_compositor_frame_ts,
  frame.viz_receive_compositor_frame_dur,
  -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
  -- On `viz_compositor_utid`.
  -- No applicable slice id (duration between two slices).
  frame.viz_receive_compositor_frame_end_ts,
  frame.viz_wait_for_draw_dur,
  -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
  -- On `viz_compositor_utid`.
  frame.viz_draw_and_swap_slice_id,
  frame.viz_draw_and_swap_ts,
  frame.viz_draw_and_swap_dur,
  -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
  -- No applicable utid (duration between two threads).
  frame.viz_send_buffer_swap_slice_id,
  frame.viz_send_buffer_swap_end_ts,
  frame.viz_to_gpu_delay_dur,
  -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
  frame.viz_gpu_thread_utid,
  frame.viz_swap_buffers_slice_id,
  frame.viz_swap_buffers_ts,
  frame.viz_swap_buffers_dur,
  -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
  frame.viz_swap_buffers_end_ts,
  frame.viz_swap_buffers_to_latch_dur,
  -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
  frame.latch_timestamp,
  frame.viz_latch_to_swap_end_dur,
  -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
  frame.swap_end_timestamp,
  frame.swap_end_to_presentation_dur,
  -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
  frame.presentation_timestamp
FROM chrome_scroll_update_input_info AS input
LEFT JOIN chrome_scroll_update_frame_info AS frame
ON input.presented_in_frame_id = frame.id;

-- Source of truth for the definition of the stages of a scroll. Mainly intended
-- for visualization purposes (e.g. in Chrome Scroll Jank plugin).
CREATE PERFETTO TABLE chrome_scroll_update_info_step_templates(
  -- The name of a stage of a scroll.
  step_name STRING,
  -- The name of the column in `chrome_scroll_update_info` which contains the
  -- timestamp of the stage.
  ts_column_name STRING,
  -- The name of the column in `chrome_scroll_update_info` which contains the
  -- duration of the stage. NULL if the stage doesn't have a duration.
  dur_column_name STRING
) AS
WITH steps(step_name, ts_column_name, dur_column_name)
AS (
  VALUES
  (
    'GenerationToBrowserMain',
    'generation_ts',
    'generation_to_browser_main_dur'
  ),
  (
    'TouchMoveProcessing',
    'touch_move_received_ts',
    'touch_move_processing_dur'
  ),
  (
    'ScrollUpdateProcessing',
    'scroll_update_created_ts',
    'scroll_update_processing_dur'
  ),
  (
    'BrowserMainToRendererCompositor',
    'scroll_update_created_end_ts',
    'browser_to_compositor_delay_dur'
  ),
  (
    'RendererCompositorDispatch',
    'compositor_dispatch_ts',
    'compositor_dispatch_dur'
  ),
  (
    'RendererCompositorDispatchToOnBeginFrame',
    'compositor_dispatch_end_ts',
    'compositor_dispatch_to_on_begin_frame_delay_dur'
  ),
  (
    'RendererCompositorBeginFrame',
    'compositor_on_begin_frame_ts',
    'compositor_on_begin_frame_dur'
  ),
  (
    'RendererCompositorBeginToGenerateFrame',
    'compositor_on_begin_frame_end_ts',
    'compositor_on_begin_frame_to_generation_delay_dur'
  ),
  (
    'RendererCompositorGenerateToSubmitFrame',
    'compositor_generate_compositor_frame_ts',
    'compositor_generate_frame_to_submit_frame_dur'
  ),
  (
    'RendererCompositorSubmitFrame',
    'compositor_submit_compositor_frame_ts',
    'compositor_submit_frame_dur'
  ),
  (
    'RendererCompositorToViz',
    'compositor_submit_compositor_frame_end_ts',
    'compositor_to_viz_delay_dur'
  ),
  (
    'VizReceiveFrame',
    'viz_receive_compositor_frame_ts',
    'viz_receive_compositor_frame_dur'
  ),
  (
    'VizReceiveToDrawFrame',
    'viz_receive_compositor_frame_end_ts',
    'viz_wait_for_draw_dur'
  ),
  (
    'VizDrawToSwapFrame',
    'viz_draw_and_swap_ts',
    'viz_draw_and_swap_dur'
  ),
  (
    'VizToGpu',
    'viz_send_buffer_swap_end_ts',
    'viz_to_gpu_delay_dur'
  ),
  (
    'VizSwapBuffers',
    'viz_swap_buffers_ts',
    'viz_swap_buffers_dur'
  ),
  (
    'VizSwapBuffersToLatch',
    'viz_swap_buffers_end_ts',
    'viz_swap_buffers_to_latch_dur'
  ),
  (
    'VizLatchToSwapEnd',
    'latch_timestamp',
    'viz_latch_to_swap_end_dur'
  ),
  (
    'VizSwapEndToPresentation',
    'swap_end_timestamp',
    'swap_end_to_presentation_dur'
  ),
  (
    'Presentation',
    'presentation_timestamp',
    NULL
  )
)
SELECT step_name, ts_column_name, dur_column_name
FROM steps;
