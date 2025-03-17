-- Copyright 2024 The Chromium Authors
-- Use of this source code is governed by a BSD-style license that can be
-- found in the LICENSE file.

INCLUDE PERFETTO MODULE slices.with_context;

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
  id LONG,
  -- The start timestamp of the slice/step.
  ts TIMESTAMP,
  -- The duration of the slice/step.
  dur DURATION,
  -- Step name of the `Graphics.Pipeline` slice.
  step STRING,
  -- Id of the graphics pipeline, pre-surface aggregation.
  surface_frame_trace_id LONG,
  -- Utid of the thread where this slice exists.
  utid LONG,
  -- Start time of the parent Chrome scheduler task (if any) of this step.
  task_start_time_ts TIMESTAMP)
AS
WITH
-- Same places in Chromium (e.g. WebView) emit -1 as the `surface_frame_trace_id`,
-- which blows up the joins on that value. Replace them with NULLs to avoid that.
raw_data AS (
  SELECT
    id,
    ts,
    dur,
    extract_arg(arg_set_id, 'chrome_graphics_pipeline.step') AS step,
    extract_arg(arg_set_id, 'chrome_graphics_pipeline.surface_frame_trace_id')
      AS surface_frame_trace_raw_id,
    utid,
    ts - (
      EXTRACT_ARG(
        thread_slice.arg_set_id,
        'current_task.event_offset_from_task_start_time_us') * 1000
    ) AS task_start_time_ts
  FROM thread_slice
  WHERE name = 'Graphics.Pipeline' AND surface_frame_trace_raw_id IS NOT NULL
)
SELECT
  id,
  ts,
  dur,
  step,
  NULLIF(surface_frame_trace_raw_id, -1) AS surface_frame_trace_id,
  utid,
  task_start_time_ts
FROM raw_data;

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
  id LONG,
  -- The start timestamp of the slice/step.
  ts TIMESTAMP,
  -- The duration of the slice/step.
  dur DURATION,
  -- Step name of the `Graphics.Pipeline` slice.
  step STRING,
  -- Id of the graphics pipeline, post-surface aggregation.
  display_trace_id LONG,
  -- Utid of the thread where this slice exists.
  utid LONG,
  -- Start time of the parent Chrome scheduler task (if any) of this step.
  task_start_time_ts TIMESTAMP)
AS
WITH steps_with_potential_duplicates AS (
    SELECT
      id,
      ts,
      dur,
      extract_arg(arg_set_id, 'chrome_graphics_pipeline.step') AS step,
      extract_arg(arg_set_id, 'chrome_graphics_pipeline.display_trace_id')
        AS display_trace_id,
      utid,
      ts - (
        EXTRACT_ARG(
          thread_slice.arg_set_id,
          'current_task.event_offset_from_task_start_time_us'
        ) * 1000
      ) AS task_start_time_ts
    FROM thread_slice
    WHERE name = 'Graphics.Pipeline' AND display_trace_id IS NOT NULL
), steps_with_ordering AS (
  SELECT
    *,
    ROW_NUMBER() OVER (
      -- Partition the steps so that, if the same step (for the same graphics
      -- pipeline) was emitted more than once (e.g. due to b:390610512), the
      -- step ends up in the same partition as all its duplicates. This will
      -- enable us to deduplicate the steps later.
      PARTITION BY display_trace_id, step, utid
      -- If there are multiple STEP_DRAW_AND_SWAP or multiple
      -- STEP_SURFACE_AGGREGATION steps, we assume that all duplicates except
      -- the last one were cancelled, so we only care about the last
      -- STEP_DRAW_AND_SWAP/STEP_SURFACE_AGGREGATION step. We don't have any
      -- preference for other steps but, for the sake of determinism and
      -- consistency, let's always pick the last step.
      ORDER BY ts DESC
    ) AS ordering_within_partition
  FROM steps_with_potential_duplicates
)
SELECT
  id,
  ts,
  dur,
  step,
  display_trace_id,
  utid,
  task_start_time_ts
FROM steps_with_ordering
-- This is where we actually remove duplicate steps.
WHERE ordering_within_partition = 1;

-- Links surface frames (`chrome_graphics_pipeline_surface_frame_steps`) to the
-- the first display frame (`chrome_graphics_pipeline_display_frame_steps`) into
-- which it was included. As an display frame usually aggregates frames from
-- multiple surfaces, multiple `surface_frame_trace_id`s will correspond to one
-- `display_trace_id`.
CREATE PERFETTO TABLE chrome_surface_frame_id_to_first_display_id(
  -- Id of the graphics pipeline, pre-surface aggregation.
  surface_frame_trace_id LONG,
  -- Id of the graphics pipeline, post-surface aggregation.
  display_trace_id LONG)
AS
WITH aggregations AS (
  SELECT
    args.int_value AS surface_frame_trace_id,
    display_trace_id,
    slice.ts
  FROM chrome_graphics_pipeline_display_frame_steps step
  JOIN slice USING (id)
  JOIN args USING (arg_set_id)
  WHERE
    step.step = 'STEP_SURFACE_AGGREGATION'
    AND args.flat_key
      = 'chrome_graphics_pipeline.aggregated_surface_frame_trace_ids'
)
SELECT
  surface_frame_trace_id,
  FIRST_VALUE(display_trace_id) OVER (
    PARTITION BY surface_frame_trace_id
    ORDER BY ts
  ) AS display_trace_id
FROM aggregations
GROUP BY surface_frame_trace_id;

-- Links inputs (`chrome_input_pipeline_steps.latency_id`) to the surface frame
-- (`chrome_graphics_pipeline_surface_frame_steps`) to which they correspond.
-- In other words, in general, multiple `latency_id`s will correspond to one
-- `surface_frame_trace_id`.
CREATE PERFETTO TABLE chrome_graphics_pipeline_inputs_to_surface_frames(
  -- Id corresponding to the input pipeline.
  latency_id LONG,
  -- Id of the graphics pipeline, post-surface aggregation.
  surface_frame_trace_id LONG)
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
