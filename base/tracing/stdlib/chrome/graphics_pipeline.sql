-- Copyright 2024 The Chromium Authors
-- Use of this source code is governed by a BSD-style license that can be
-- found in the LICENSE file.

INCLUDE PERFETTO MODULE deprecated.v42.common.slices;

-- `Graphics.Pipeline` steps corresponding to work done by a Viz client to
-- produce a frame (i.e. before surface aggregation). Covers steps:
--   * STEP_ISSUE_BEGIN_FRAME
--   * STEP_RECEIVE_BEGIN_FRAME
--   * STEP_GENERATE_RENDER_PASS
--   * STEP_GENERATE_COMPOSITOR_FRAME
--   * STEP_SUBMIT_COMPOSITOR_FRAME
--   * STEP_RECEIVE_COMPOSITOR_FRAME
--   * STEP_RECEIVE_BEGIN_FRAME_DISCARD
--   * STEP_DID_NOT_PRODUCE_FRAME
--   * STEP_DID_NOT_PRODUCE_COMPOSITOR_FRAME
CREATE PERFETTO TABLE chrome_graphics_pipeline_surface_frame_steps(
  -- Slice Id of the `Graphics.Pipeline` slice.
  id INT,
  -- The start timestamp of the slice/step.
  ts INT,
  -- The duration of the slice/step.
  dur INT,
  -- Step name of the `Graphics.Pipeline` slice.
  step STRING,
  -- Id of the graphics pipeline, pre-surface aggregation.
  surface_frame_trace_id INT,
  -- Utid of the thread where this slice exists.
  utid INT)
AS
SELECT
  id,
  ts,
  dur,
  extract_arg(arg_set_id, 'chrome_graphics_pipeline.step') AS step,
  extract_arg(arg_set_id, 'chrome_graphics_pipeline.surface_frame_trace_id')
    AS surface_frame_trace_id,
  utid
FROM thread_slice
WHERE name = 'Graphics.Pipeline' AND surface_frame_trace_id IS NOT NULL;

-- `Graphics.Pipeline` steps corresponding to work done on creating and
-- presenting one frame during/after surface aggregation. Covers steps:
--   * STEP_DRAW_AND_SWAP
--   * STEP_SURFACE_AGGREGATION
--   * STEP_SEND_BUFFER_SWAP
--   * STEP_BUFFER_SWAP_POST_SUBMIT
--   * STEP_FINISH_BUFFER_SWAP
--   * STEP_SWAP_BUFFERS_ACK
CREATE PERFETTO TABLE chrome_graphics_pipeline_display_frame_steps(
  -- Slice Id of the `Graphics.Pipeline` slice.
  id INT,
  -- The start timestamp of the slice/step.
  ts INT,
  -- The duration of the slice/step.
  dur INT,
  -- Step name of the `Graphics.Pipeline` slice.
  step STRING,
  -- Id of the graphics pipeline, post-surface aggregation.
  display_trace_id INT,
  -- Utid of the thread where this slice exists.
  utid INT)
AS
SELECT
  id,
  ts,
  dur,
  extract_arg(arg_set_id, 'chrome_graphics_pipeline.step') AS step,
  extract_arg(arg_set_id, 'chrome_graphics_pipeline.display_trace_id')
    AS display_trace_id,
  utid
FROM thread_slice
WHERE name = 'Graphics.Pipeline' AND display_trace_id IS NOT NULL;

-- Links surface frames (`chrome_graphics_pipeline_surface_frame_steps`) to the
-- display frame (`chrome_graphics_pipeline_display_frame_steps`) into which
-- they are merged. In other words, in general, multiple
-- `surface_frame_trace_id`s will correspond to one `display_trace_id`.
CREATE PERFETTO TABLE chrome_graphics_pipeline_aggregated_frames(
  -- Id of the graphics pipeline, pre-surface aggregation.
  surface_frame_trace_id INT,
  -- Id of the graphics pipeline, post-surface aggregation.
  display_trace_id INT)
AS
SELECT
  args.int_value AS surface_frame_trace_id,
  display_trace_id
FROM chrome_graphics_pipeline_display_frame_steps step
JOIN slice
  USING (id)
JOIN args
  USING (arg_set_id)
WHERE
  step.step = 'STEP_SURFACE_AGGREGATION'
  AND args.flat_key
    = 'chrome_graphics_pipeline.aggregated_surface_frame_trace_ids';

-- Links inputs (`chrome_input_pipeline_steps.latency_id`) to the surface frame
-- (`chrome_graphics_pipeline_surface_frame_steps`) to which they correspond.
-- In other words, in general, multiple `latency_id`s will correspond to one
-- `surface_frame_trace_id`.
CREATE PERFETTO TABLE chrome_graphics_pipeline_inputs_to_surface_frames(
  -- Id corresponding to the input pipeline.
  latency_id INT,
  -- Id of the graphics pipeline, post-surface aggregation.
  surface_frame_trace_id INT)
AS
SELECT
  args.int_value AS latency_id,
  surface_frame_trace_id
FROM chrome_graphics_pipeline_surface_frame_steps step
JOIN slice
  USING (id)
JOIN args
  USING (arg_set_id)
WHERE
  step.step = 'STEP_SUBMIT_COMPOSITOR_FRAME'
  AND args.flat_key = 'chrome_graphics_pipeline.latency_ids';
