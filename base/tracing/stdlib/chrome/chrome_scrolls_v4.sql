-- Copyright 2026 The Chromium Authors
-- Use of this source code is governed by a BSD-style license that can be
-- found in the LICENSE file.

INCLUDE PERFETTO MODULE chrome.chrome_scrolls;

INCLUDE PERFETTO MODULE chrome.scroll_jank_v4;

-- A helper macro which evaluates to `expression` if the first scroll update in
-- a frame is real. Otherwise, evaluates to NULL. Should be only used in
-- `chrome_scroll_frame_info_v4`.
CREATE PERFETTO MACRO _if_real_first_scroll_update(
    expression Expr
)
RETURNS Expr AS
iif(results.first_scroll_update_type = 'REAL', $expression, NULL);

-- A helper macro which evaluates to `expression` if a frame is damaging.
-- Otherwise, evaluates to NULL. Should be only used in
-- `chrome_scroll_frame_info_v4`.
CREATE PERFETTO MACRO _if_damaging_frame(
    expression Expr
)
RETURNS Expr AS
iif(results.damage_type = 'DAMAGING', $expression, NULL);

-- Helper macro to compute the delta between the duration of this frame's and
-- previous frame's `stage`. Should be only used in
-- `chrome_scroll_frame_info_v4`.
CREATE PERFETTO MACRO _stage_dur_delta_v4(
    stage ColumnName
)
RETURNS Expr AS
$stage - lag($stage) OVER (PARTITION BY info.scroll_id ORDER BY results.ts);

-- A list of all presented Chrome frames which contain scroll updates together
-- with additional metadata, pipeline stages breakdown and jank classification
-- according to the scroll model of Chrome's scroll jank v4 metric.
CREATE PERFETTO TABLE chrome_scroll_frame_info_v4 (
  -- Slice ID of the 'ScrollJankV4' slice, which represents a frame presented by
  -- Chrome that contains at least one scroll update. Can be joined with
  -- `chrome_scroll_jank_v4_results.id`.
  id JOINID(slice.id),
  -- Id of the `LatencyInfo.Flow` slices corresponding to the first scroll
  -- update in this frame. Can be joined with
  -- `chrome_gesture_scroll_updates.id`, `chrome_scroll_update_info.id`,
  -- `chrome_scroll_update_refs.scroll_update_latency_id` and many other tables.
  scroll_update_latency_id LONG,
  -- Id of the scroll this frame belongs to. Can be joined with
  -- `chrome_event_latency.scroll_id`.
  scroll_id LONG,
  -- The VSync interval that this frame was produced for according to the
  -- BeginFrameArgs.
  vsync_interval_dur DURATION,
  -- Whether this frame is janky based on the
  -- `Event.ScrollJank.DelayedFramesPercentage4.FixedWindow` metric.
  is_janky BOOL,
  -- Whether the corresponding scroll is inertial (fling).
  is_inertial BOOL,
  -- The position of the frame in a scroll (starting from 1).
  frame_index_in_scroll LONG,
  -- The absolute total raw (unpredicted) delta of all real scroll updates
  -- included in this frame (in pixels). NULL if the first scroll update in this
  -- frame is synthetic.
  real_abs_total_raw_delta_pixels DOUBLE,
  -- Duration from the start of the browser process to the input generation
  -- timestamp of the first scroll update in this frame. NULL if the first
  -- scroll update in this frame is synthetic.
  browser_uptime_dur DURATION,
  -- Input generation timestamp (from the Android system) for the first scroll
  -- update in this frame. NULL if the first scroll update in this frame is
  -- synthetic.
  first_input_generation_ts TIMESTAMP,
  -- Duration from the generation timestamp to the end of InputReader's work.
  -- Only populated when atrace 'input' category is enabled. NULL if the first
  -- scroll update is synthetic.
  input_reader_dur DURATION,
  -- Duration of InputDispatcher's work. Only populated when atrace 'input'
  -- category is enabled. NULL if the first scroll update in this frame is
  -- synthetic.
  input_dispatcher_dur DURATION,
  -- Duration from the previous input (last input that wasn't part of this
  -- frame) to the first input in this frame.
  previous_last_input_to_first_input_generation_dur DURATION,
  -- Utid for the browser main thread, on which Chrome received the first scroll
  -- update in this frame. NULL if the first scroll update in this frame is
  -- synthetic.
  browser_utid JOINID(thread.id),
  -- Duration from input generation to when the browser received the first
  -- scroll update in this frame. NULL if the first scroll update in this frame
  -- is synthetic.
  first_input_generation_to_browser_main_dur DURATION,
  -- Difference between `first_input_generation_to_browser_main_dur` for this
  -- frame and the previous frame in the same scroll. NULL if the first scroll
  -- update in either frame is synthetic.
  first_input_generation_to_browser_main_delta_dur DURATION,
  -- Slice id for the `STEP_SEND_INPUT_EVENT_UI` slice for the `TouchMove` event
  -- associated with the first scroll update in this frame. NULL if the first
  -- scroll update in this frame is synthetic.
  first_input_touch_move_received_slice_id JOINID(slice.id),
  -- Timestamp for the `STEP_SEND_INPUT_EVENT_UI` slice for the `TouchMove`
  -- event associated with the first scroll update in this frame. NULL if the
  -- first scroll update in this frame is synthetic.
  first_input_touch_move_received_ts TIMESTAMP,
  -- Duration for processing the `TouchMove` event associated with the first
  -- scroll update in this frame. NULL if the first scroll update in this frame
  -- is synthetic.
  first_input_touch_move_processing_dur DURATION,
  -- Difference between `first_input_touch_move_processing_dur` for this frame
  -- and the previous frame in the same scroll. NULL if the first scroll update
  -- in either frame is synthetic.
  first_input_touch_move_processing_delta_dur DURATION,
  -- Slice id for the `STEP_SEND_INPUT_EVENT_UI` slice which created the
  -- `GestureScrollUpdate` event from the `TouchMove` event for the first scroll
  -- update in this frame. NULL if the first scroll update in this frame is
  -- synthetic.
  first_input_scroll_update_created_slice_id JOINID(slice.id),
  -- Timestamp for the `STEP_SEND_INPUT_EVENT_UI` slice which created the
  -- `GestureScrollUpdate` event from the `TouchMove` event for the first scroll
  -- update in this frame. NULL if the first scroll update in this frame is
  -- synthetic.
  first_input_scroll_update_created_ts TIMESTAMP,
  -- Duration for creating the `GestureScrollUpdate` event from the `TouchMove`
  -- event for the first scroll update in this frame. NULL if the first scroll
  -- update in this frame is synthetic.
  first_input_scroll_update_processing_dur DURATION,
  -- Difference between `first_input_scroll_update_processing_dur` for this
  -- frame and the previous frame in the same scroll. NULL if the first scroll
  -- update in either frame is synthetic.
  first_input_scroll_update_processing_delta_dur DURATION,
  -- End timestamp for the `STEP_SEND_INPUT_EVENT_UI` slice which created the
  -- `GestureScrollUpdate` event from the `TouchMove` event for the first scroll
  -- update in this frame. NULL if the first scroll update in this frame is
  -- synthetic.
  first_input_scroll_update_created_end_ts TIMESTAMP,
  -- Duration between the browser and compositor dispatch for the first scroll
  -- update in this frame. NULL if the first scroll update in this frame is
  -- synthetic.
  first_input_browser_to_compositor_delay_dur DURATION,
  -- Difference between `first_input_browser_to_compositor_delay_dur` for this
  -- frame and the previous frame in the same scroll. NULL if either scroll
  -- update in this frame is synthetic.
  first_input_browser_to_compositor_delay_delta_dur DURATION,
  -- Utid for the renderer compositor thread.
  compositor_utid JOINID(thread.id),
  -- Slice id for the `STEP_HANDLE_INPUT_EVENT_IMPL` slice associated with the
  -- first scroll update in this frame.
  first_input_compositor_dispatch_slice_id JOINID(slice.id),
  -- Timestamp for the `STEP_HANDLE_INPUT_EVENT_IMPL` slice associated with the
  -- first scroll update in this frame or the containing task (if available).
  first_input_compositor_dispatch_ts TIMESTAMP,
  -- Duration for the compositor dispatch for the first scroll update in this
  -- frame.
  first_input_compositor_dispatch_dur DURATION,
  -- Difference between `first_input_compositor_dispatch_dur` for this frame and
  -- the previous frame in the same scroll.
  first_input_compositor_dispatch_delta_dur DURATION,
  -- End timestamp for the `STEP_HANDLE_INPUT_EVENT_IMPL` slice associated with
  -- the first scroll update in this frame.
  first_input_compositor_dispatch_end_ts TIMESTAMP,
  -- Duration between the compositor dispatch and the "OnBeginFrame" work for
  -- the first input in this frame. NULL if this frame is non-damaging.
  first_input_compositor_dispatch_to_on_begin_frame_delay_dur DURATION,
  -- Difference between
  -- `first_input_compositor_dispatch_to_on_begin_frame_delay_dur` for this
  -- frame and the previous frame in the same scroll. NULL if either frame is
  -- non-damaging.
  first_input_compositor_dispatch_to_on_begin_frame_delay_delta_dur DURATION,
  -- Slice id for the `STEP_RESAMPLE_SCROLL_EVENTS` slice. NULL if this frame is
  -- non-damaging.
  compositor_resample_slice_id JOINID(slice.id),
  -- Slice id for the `STEP_DID_HANDLE_INPUT_AND_OVERSCROLL` slice. NULL if this
  -- frame is non-damaging.
  compositor_coalesced_input_handled_slice_id JOINID(slice.id),
  -- Start timestamp for work done on the input during "OnBeginFrame". NULL if
  -- this frame is non-damaging.
  compositor_on_begin_frame_ts TIMESTAMP,
  -- Duration of the "OnBeginFrame" work for this frame. NULL if this frame is
  -- non-damaging.
  compositor_on_begin_frame_dur DURATION,
  -- Difference between `compositor_on_begin_frame_dur` for this frame and the
  -- previous frame in the same scroll. NULL if either frame is non-damaging.
  compositor_on_begin_frame_delta_dur DURATION,
  -- End timestamp for work done on the input during "OnBeginFrame". NULL if
  -- this frame is non-damaging.
  compositor_on_begin_frame_end_ts TIMESTAMP,
  -- Duration between the "OnBeginFrame" work and the generation of this frame.
  -- NULL if this frame is non-damaging.
  compositor_on_begin_frame_to_generation_delay_dur DURATION,
  -- Difference between `compositor_on_begin_frame_to_generation_delay_dur` for
  -- this frame and the previous frame in the same scroll. NULL if either frame
  -- is non-damaging.
  compositor_on_begin_frame_to_generation_delay_delta_dur DURATION,
  -- Slice id for the `STEP_GENERATE_COMPOSITOR_FRAME` slice. NULL if this frame
  -- is non-damaging.
  compositor_generate_compositor_frame_slice_id JOINID(slice.id),
  -- Timestamp for the `STEP_GENERATE_COMPOSITOR_FRAME` slice or the containing
  -- task (if available). NULL if this frame is non-damaging.
  compositor_generate_compositor_frame_ts TIMESTAMP,
  -- Duration between the generation and submission of this frame. NULL if this
  -- frame is non-damaging.
  compositor_generate_frame_to_submit_frame_dur DURATION,
  -- Difference between `compositor_generate_frame_to_submit_frame_dur` for this
  -- frame and the previous frame in the same scroll. NULL if either frame is
  -- non-damaging.
  compositor_generate_frame_to_submit_frame_delta_dur DURATION,
  -- Slice id for the `STEP_SUBMIT_COMPOSITOR_FRAME` slice. NULL if this frame
  -- is non-damaging.
  compositor_submit_compositor_frame_slice_id JOINID(slice.id),
  -- Timestamp for the `STEP_SUBMIT_COMPOSITOR_FRAME` slice. NULL if this frame
  -- is non-damaging.
  compositor_submit_compositor_frame_ts TIMESTAMP,
  -- Duration for submitting this frame. NULL if this frame is non-damaging.
  compositor_submit_frame_dur DURATION,
  -- Difference between `compositor_submit_frame_dur` for this frame and the
  -- previous frame in the same scroll. NULL if either frame is non-damaging.
  compositor_submit_frame_delta_dur DURATION,
  -- End timestamp for the `STEP_SUBMIT_COMPOSITOR_FRAME` slice. NULL if this
  -- frame is non-damaging.
  compositor_submit_compositor_frame_end_ts TIMESTAMP,
  -- Utid for the viz compositor thread. NULL if this frame is non-damaging.
  viz_compositor_utid JOINID(thread.id),
  -- Delay when a compositor frame is sent from the renderer to viz. NULL if
  -- this frame is non-damaging.
  compositor_to_viz_delay_dur DURATION,
  -- Difference between `compositor_to_viz_delay_dur` for this frame and the
  -- previous frame in the same scroll. NULL if either frame is non-damaging.
  compositor_to_viz_delay_delta_dur DURATION,
  -- Slice id for the `STEP_RECEIVE_COMPOSITOR_FRAME` slice. NULL if this frame
  -- is non-damaging.
  viz_receive_compositor_frame_slice_id JOINID(slice.id),
  -- Timestamp for the `STEP_RECEIVE_COMPOSITOR_FRAME` slice or the containing
  -- task (if available). NULL if this frame is non-damaging.
  viz_receive_compositor_frame_ts TIMESTAMP,
  -- Duration of the viz work done on receiving the compositor frame. NULL if
  -- this frame is non-damaging.
  viz_receive_compositor_frame_dur DURATION,
  -- Difference between `viz_receive_compositor_frame_dur` for this frame and
  -- the previous frame in the same scroll. NULL if either frame is
  -- non-damaging.
  viz_receive_compositor_frame_delta_dur DURATION,
  -- End timestamp for the `STEP_RECEIVE_COMPOSITOR_FRAME` slice. NULL if this
  -- frame is non-damaging.
  viz_receive_compositor_frame_end_ts TIMESTAMP,
  -- Duration between viz receiving the compositor frame to frame draw. NULL if
  -- this frame is non-damaging.
  viz_wait_for_draw_dur DURATION,
  -- Difference between `viz_wait_for_draw_dur` for this frame and the previous
  -- frame in the same scroll. NULL if either frame is non-damaging.
  viz_wait_for_draw_delta_dur DURATION,
  -- Slice id for the `STEP_DRAW_AND_SWAP` slice. NULL if this frame is
  -- non-damaging.
  viz_draw_and_swap_slice_id JOINID(slice.id),
  -- Timestamp for the `STEP_DRAW_AND_SWAP` slice or the containing task (if
  -- available). NULL if this frame is non-damaging.
  viz_draw_and_swap_ts TIMESTAMP,
  -- Duration of the viz drawing/swapping work for this frame. NULL if this
  -- frame is non-damaging.
  viz_draw_and_swap_dur DURATION,
  -- Difference between `viz_draw_and_swap_dur` for this frame and the previous
  -- frame in the same scroll. NULL if either frame is non-damaging.
  viz_draw_and_swap_delta_dur DURATION,
  -- Slice id for the `STEP_SEND_BUFFER_SWAP` slice. NULL if this frame is
  -- non-damaging.
  viz_send_buffer_swap_slice_id JOINID(slice.id),
  -- End timestamp for the `STEP_SEND_BUFFER_SWAP` slice. NULL if this frame is
  -- non-damaging.
  viz_send_buffer_swap_end_ts TIMESTAMP,
  -- Utid for the viz `CompositorGpuThread`. NULL if this frame is non-damaging.
  viz_gpu_thread_utid JOINID(thread.id),
  -- Delay between viz work on compositor thread and `CompositorGpuThread`. NULL
  -- if this frame is non-damaging.
  viz_to_gpu_delay_dur DURATION,
  -- Difference between `viz_to_gpu_delay_dur` for this frame and the previous
  -- frame in the same scroll. NULL if either frame is non-damaging.
  viz_to_gpu_delay_delta_dur DURATION,
  -- Slice id for the `STEP_BUFFER_SWAP_POST_SUBMIT` slice. NULL if this frame
  -- is non-damaging.
  viz_swap_buffers_slice_id JOINID(slice.id),
  -- Timestamp for the `STEP_BUFFER_SWAP_POST_SUBMIT` slice or the containing
  -- task (if available). NULL if this frame is non-damaging.
  viz_swap_buffers_ts TIMESTAMP,
  -- Duration of frame buffer swapping work on viz. NULL if this frame is
  -- non-damaging.
  viz_swap_buffers_dur DURATION,
  -- Difference between `viz_swap_buffers_dur` for this frame and the previous
  -- frame in the same scroll. NULL if either frame is non-damaging.
  viz_swap_buffers_delta_dur DURATION,
  -- End timestamp for the `STEP_BUFFER_SWAP_POST_SUBMIT` slice. NULL if this
  -- frame is non-damaging.
  viz_swap_buffers_end_ts TIMESTAMP,
  -- Time between buffers ready until Choreographer's latch. NULL if this frame
  -- is non-damaging.
  viz_swap_buffers_to_latch_dur DURATION,
  -- Difference between `viz_swap_buffers_to_latch_dur` for this frame and the
  -- previous frame in the same scroll. NULL if either frame is non-damaging.
  viz_swap_buffers_to_latch_delta_dur DURATION,
  -- Timestamp for `EventLatency`'s `LatchToSwapEnd` step. NULL if this frame is
  -- non-damaging.
  latch_timestamp TIMESTAMP,
  -- Duration between Choreographer's latch and presentation. NULL if this frame
  -- is non-damaging.
  viz_latch_to_presentation_dur DURATION,
  -- Difference between `viz_latch_to_presentation_dur` for this frame and the
  -- previous frame in the same scroll. NULL if either frame is non-damaging.
  viz_latch_to_presentation_delta_dur DURATION,
  -- Presentation timestamp for the frame. NULL if this frame is non-damaging.
  presentation_ts TIMESTAMP
) AS
SELECT
  results.id,
  results.first_event_latency_id AS scroll_update_latency_id,
  info.scroll_id,
  results.vsync_interval AS vsync_interval_dur,
  results.is_janky,
  NOT results.real_max_abs_inertial_raw_delta_pixels IS NULL AS is_inertial,
  row_number() OVER (PARTITION BY info.scroll_id ORDER BY results.ts) AS frame_index_in_scroll,
  -- Columns which are only relevant frames whose first scroll update is REAL.
  _if_real_first_scroll_update!(results.real_abs_total_raw_delta_pixels) AS real_abs_total_raw_delta_pixels,
  _if_real_first_scroll_update!(info.browser_uptime_dur) AS browser_uptime_dur,
  _if_real_first_scroll_update!(info.generation_ts) AS first_input_generation_ts,
  _if_real_first_scroll_update!(info.input_reader_dur) AS input_reader_dur,
  _if_real_first_scroll_update!(info.input_dispatcher_dur) AS input_dispatcher_dur,
  _if_real_first_scroll_update!(info.generation_ts - MAX(results.real_last_input_generation_ts) OVER (PARTITION BY info.scroll_id ORDER BY results.ts ROWS BETWEEN UNBOUNDED PRECEDING AND 1 PRECEDING)) AS previous_last_input_to_first_input_generation_dur,
  _if_real_first_scroll_update!(info.browser_utid) AS browser_utid,
  _if_real_first_scroll_update!(info.generation_to_browser_main_dur) AS first_input_generation_to_browser_main_dur,
  _if_real_first_scroll_update!(_stage_dur_delta_v4!(info.generation_to_browser_main_dur)) AS first_input_generation_to_browser_main_delta_dur,
  _if_real_first_scroll_update!(info.touch_move_received_slice_id) AS first_input_touch_move_received_slice_id,
  _if_real_first_scroll_update!(info.touch_move_received_ts) AS first_input_touch_move_received_ts,
  _if_real_first_scroll_update!(info.touch_move_processing_dur) AS first_input_touch_move_processing_dur,
  _if_real_first_scroll_update!(_stage_dur_delta_v4!(info.touch_move_processing_dur)) AS first_input_touch_move_processing_delta_dur,
  _if_real_first_scroll_update!(info.scroll_update_created_slice_id) AS first_input_scroll_update_created_slice_id,
  _if_real_first_scroll_update!(info.scroll_update_created_ts) AS first_input_scroll_update_created_ts,
  _if_real_first_scroll_update!(info.scroll_update_processing_dur) AS first_input_scroll_update_processing_dur,
  _if_real_first_scroll_update!(_stage_dur_delta_v4!(info.scroll_update_processing_dur)) AS first_input_scroll_update_processing_delta_dur,
  _if_real_first_scroll_update!(info.scroll_update_created_end_ts) AS first_input_scroll_update_created_end_ts,
  _if_real_first_scroll_update!(info.browser_to_compositor_delay_dur) AS first_input_browser_to_compositor_delay_dur,
  _if_real_first_scroll_update!(_stage_dur_delta_v4!(info.browser_to_compositor_delay_dur)) AS first_input_browser_to_compositor_delay_delta_dur,
  -- A few columns which are relevant to ALL scroll frames.
  info.compositor_utid,
  info.compositor_dispatch_slice_id AS first_input_compositor_dispatch_slice_id,
  info.compositor_dispatch_ts AS first_input_compositor_dispatch_ts,
  info.compositor_dispatch_dur AS first_input_compositor_dispatch_dur,
  _stage_dur_delta_v4!(info.compositor_dispatch_dur) AS first_input_compositor_dispatch_delta_dur,
  info.compositor_dispatch_end_ts AS first_input_compositor_dispatch_end_ts,
  -- The remaining columns below are only relevant to DAMAGING frames.
  _if_damaging_frame!(info.compositor_dispatch_to_on_begin_frame_delay_dur) AS first_input_compositor_dispatch_to_on_begin_frame_delay_dur,
  _if_damaging_frame!(_stage_dur_delta_v4!(info.compositor_dispatch_to_on_begin_frame_delay_dur)) AS first_input_compositor_dispatch_to_on_begin_frame_delay_delta_dur,
  _if_damaging_frame!(info.compositor_resample_slice_id) AS compositor_resample_slice_id,
  _if_damaging_frame!(info.compositor_coalesced_input_handled_slice_id) AS compositor_coalesced_input_handled_slice_id,
  _if_damaging_frame!(info.compositor_on_begin_frame_ts) AS compositor_on_begin_frame_ts,
  _if_damaging_frame!(info.compositor_on_begin_frame_dur) AS compositor_on_begin_frame_dur,
  _if_damaging_frame!(_stage_dur_delta_v4!(info.compositor_on_begin_frame_dur)) AS compositor_on_begin_frame_delta_dur,
  _if_damaging_frame!(info.compositor_on_begin_frame_end_ts) AS compositor_on_begin_frame_end_ts,
  _if_damaging_frame!(info.compositor_on_begin_frame_to_generation_delay_dur) AS compositor_on_begin_frame_to_generation_delay_dur,
  _if_damaging_frame!(_stage_dur_delta_v4!(info.compositor_on_begin_frame_to_generation_delay_dur)) AS compositor_on_begin_frame_to_generation_delay_delta_dur,
  _if_damaging_frame!(info.compositor_generate_compositor_frame_slice_id) AS compositor_generate_compositor_frame_slice_id,
  _if_damaging_frame!(info.compositor_generate_compositor_frame_ts) AS compositor_generate_compositor_frame_ts,
  _if_damaging_frame!(info.compositor_generate_frame_to_submit_frame_dur) AS compositor_generate_frame_to_submit_frame_dur,
  _if_damaging_frame!(_stage_dur_delta_v4!(info.compositor_generate_frame_to_submit_frame_dur)) AS compositor_generate_frame_to_submit_frame_delta_dur,
  _if_damaging_frame!(compositor_submit_compositor_frame_slice_id) AS compositor_submit_compositor_frame_slice_id,
  _if_damaging_frame!(compositor_submit_compositor_frame_ts) AS compositor_submit_compositor_frame_ts,
  _if_damaging_frame!(info.compositor_submit_frame_dur) AS compositor_submit_frame_dur,
  _if_damaging_frame!(_stage_dur_delta_v4!(info.compositor_submit_frame_dur)) AS compositor_submit_frame_delta_dur,
  _if_damaging_frame!(info.compositor_submit_compositor_frame_end_ts) AS compositor_submit_compositor_frame_end_ts,
  _if_damaging_frame!(info.viz_compositor_utid) AS viz_compositor_utid,
  _if_damaging_frame!(info.compositor_to_viz_delay_dur) AS compositor_to_viz_delay_dur,
  _if_damaging_frame!(_stage_dur_delta_v4!(info.compositor_to_viz_delay_dur)) AS compositor_to_viz_delay_delta_dur,
  _if_damaging_frame!(info.viz_receive_compositor_frame_slice_id) AS viz_receive_compositor_frame_slice_id,
  _if_damaging_frame!(info.viz_receive_compositor_frame_ts) AS viz_receive_compositor_frame_ts,
  _if_damaging_frame!(info.viz_receive_compositor_frame_dur) AS viz_receive_compositor_frame_dur,
  _if_damaging_frame!(_stage_dur_delta_v4!(info.viz_receive_compositor_frame_dur)) AS viz_receive_compositor_frame_delta_dur,
  _if_damaging_frame!(info.viz_receive_compositor_frame_end_ts) AS viz_receive_compositor_frame_end_ts,
  _if_damaging_frame!(info.viz_wait_for_draw_dur) AS viz_wait_for_draw_dur,
  _if_damaging_frame!(_stage_dur_delta_v4!(info.viz_wait_for_draw_dur)) AS viz_wait_for_draw_delta_dur,
  _if_damaging_frame!(info.viz_draw_and_swap_slice_id) AS viz_draw_and_swap_slice_id,
  _if_damaging_frame!(info.viz_draw_and_swap_ts) AS viz_draw_and_swap_ts,
  _if_damaging_frame!(info.viz_draw_and_swap_dur) AS viz_draw_and_swap_dur,
  _if_damaging_frame!(_stage_dur_delta_v4!(info.viz_draw_and_swap_dur)) AS viz_draw_and_swap_delta_dur,
  _if_damaging_frame!(info.viz_send_buffer_swap_slice_id) AS viz_send_buffer_swap_slice_id,
  _if_damaging_frame!(info.viz_send_buffer_swap_end_ts) AS viz_send_buffer_swap_end_ts,
  _if_damaging_frame!(info.viz_gpu_thread_utid) AS viz_gpu_thread_utid,
  _if_damaging_frame!(info.viz_to_gpu_delay_dur) AS viz_to_gpu_delay_dur,
  _if_damaging_frame!(_stage_dur_delta_v4!(info.viz_to_gpu_delay_dur)) AS viz_to_gpu_delay_delta_dur,
  _if_damaging_frame!(info.viz_swap_buffers_slice_id) AS viz_swap_buffers_slice_id,
  _if_damaging_frame!(info.viz_swap_buffers_ts) AS viz_swap_buffers_ts,
  _if_damaging_frame!(info.viz_swap_buffers_dur) AS viz_swap_buffers_dur,
  _if_damaging_frame!(_stage_dur_delta_v4!(info.viz_swap_buffers_dur)) AS viz_swap_buffers_delta_dur,
  _if_damaging_frame!(info.viz_swap_buffers_end_ts) AS viz_swap_buffers_end_ts,
  _if_damaging_frame!(info.viz_swap_buffers_to_latch_dur) AS viz_swap_buffers_to_latch_dur,
  _if_damaging_frame!(_stage_dur_delta_v4!(info.viz_swap_buffers_to_latch_dur)) AS viz_swap_buffers_to_latch_delta_dur,
  _if_damaging_frame!(info.latch_timestamp) AS latch_timestamp,
  _if_damaging_frame!(info.viz_latch_to_presentation_dur) AS viz_latch_to_presentation_dur,
  _if_damaging_frame!(_stage_dur_delta_v4!(info.viz_latch_to_presentation_dur)) AS viz_latch_to_presentation_delta_dur,
  _if_damaging_frame!(info.presentation_timestamp) AS presentation_ts
FROM chrome_scroll_jank_v4_results AS results
LEFT JOIN chrome_scroll_update_info AS info
  ON info.id = results.first_event_latency_id
ORDER BY
  results.ts;