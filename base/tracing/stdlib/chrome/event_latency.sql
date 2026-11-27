-- Copyright 2024 The Chromium Authors
-- Use of this source code is governed by a BSD-style license that can be
-- found in the LICENSE file.

-- All EventLatency slices.
CREATE PERFETTO TABLE chrome_event_latencies (
  -- Slice Id for the EventLatency scroll event.
  id LONG,
  -- Slice name.
  name STRING,
  -- The start timestamp of the scroll.
  ts TIMESTAMP,
  -- The duration of the scroll.
  dur DURATION,
  -- The id of the scroll update event (aka LatencyInfo.ID).
  scroll_update_id LONG,
  -- The id of the first frame (pre-surface aggregation) which included the
  -- scroll update and was presented. NULL if:
  -- (1) the event is not a scroll update (`event_type` is NOT
  --     GESTURE_SCROLL_UPDATE, FIRST_GESTURE_SCROLL_UPDATE, or
  --     INERTIAL_GESTURE_SCROLL_UPDATE),
  -- (2) the scroll update wasn't presented (e.g. it was an overscroll) or
  -- (3) the trace comes from an old Chrome version (https://crrev.com/c/6185817
  --     was first included in version 134.0.6977.0 and was cherry-picked in
  --     version 133.0.6943.33).
  surface_frame_trace_id LONG,
  -- The id of the first frame (post-surface aggregation) which included the
  -- scroll update and was presented. NULL if:
  -- (1) the event is not a scroll update (`event_type` is NOT
  --     GESTURE_SCROLL_UPDATE, FIRST_GESTURE_SCROLL_UPDATE, or
  --     INERTIAL_GESTURE_SCROLL_UPDATE),
  -- (2) the scroll update wasn't presented (e.g. it was an overscroll) or
  -- (3) the trace comes from an old Chrome version (https://crrev.com/c/6185817
  --     was first included in version 134.0.6977.0 and was cherry-picked in
  --     version 133.0.6943.33).
  display_trace_id LONG,
  -- Whether this input event was presented.
  is_presented BOOL,
  -- EventLatency event type.
  event_type STRING,
  -- Perfetto track this slice is found on.
  track_id LONG,
  -- Vsync interval (in milliseconds).
  vsync_interval_ms DOUBLE,
  -- Whether the corresponding frame is janky based on the
  -- Event.ScrollJank.DelayedFramesPercentage.FixedWindow metric.
  is_janky_scrolled_frame BOOL,
  -- Timestamp of the BufferAvailableToBufferReady substage.
  buffer_available_timestamp LONG,
  -- Timestamp of the BufferReadyToLatch substage.
  buffer_ready_timestamp LONG,
  -- Timestamp of the LatchToSwapEnd substage (or LatchToPresentation as a
  -- fallback).
  latch_timestamp LONG,
  -- Timestamp of the SwapEndToPresentationCompositorFrame substage.
  swap_end_timestamp LONG,
  -- Frame presentation timestamp aka the timestamp of the
  -- SwapEndToPresentationCompositorFrame substage.
  -- TODO(b/341047059): temporarily use LatchToSwapEnd as a workaround if
  -- SwapEndToPresentationCompositorFrame is missing due to b/247542163.
  presentation_timestamp LONG
) AS
WITH
  -- Scan `descendant_slice` once per `EventLatency` slice and keep the first
  -- matching descendant for each substage.
  first_descendants_per_stage AS (
    SELECT
      el.id AS event_latency_id,
      CASE
        WHEN d.name GLOB 'Submit*ToPresentationCompositorFrame'
        THEN 'Submit*ToPresentationCompositorFrame'
        WHEN d.name != 'LatchToPresentation' AND d.name GLOB '*ToPresentation'
        THEN 'OtherToPresentation'
        ELSE d.name
      END AS stage,
      min(d.id) AS first_descendant_id,
      -- Note: SQLite documentation guarantees to choose `ts` and `end_ts` from
      -- a row that has `MIN(d.id)`. See
      -- https://sqlite.org/lang_select.html#bare_columns_in_an_aggregate_query.
      d.ts,
      iif(d.dur != -1, d.ts + d.dur, NULL) AS end_ts
    FROM slice AS el
    JOIN descendant_slice(el.id) AS d
    WHERE
      el.name = 'EventLatency'
      AND (
        d.name IN (
          'BufferAvailableToBufferReady',
          'BufferReadyToLatch',
          'LatchToSwapEnd',
          'LatchToPresentation',
          'SwapEndToPresentationCompositorFrame'
        )
        OR d.name GLOB 'Submit*ToPresentationCompositorFrame'
        OR d.name GLOB '*ToPresentation'
      )
    GROUP BY
      el.id,
      stage
  ),
  pivoted_stages AS (
    SELECT
      event_latency_id AS id,
      max(stage = 'Submit*ToPresentationCompositorFrame') AS is_presented,
      max(iif(stage = 'BufferAvailableToBufferReady', ts, NULL)) AS buffer_available_timestamp,
      max(iif(stage = 'BufferReadyToLatch', ts, NULL)) AS buffer_ready_timestamp,
      coalesce(
        max(iif(stage = 'LatchToSwapEnd', ts, NULL)),
        max(iif(stage = 'LatchToPresentation', ts, NULL))
      ) AS latch_timestamp,
      max(iif(stage = 'SwapEndToPresentationCompositorFrame', ts, NULL)) AS swap_end_timestamp,
      -- Presentation timestamp is the end of
      -- SwapEndToPresentationCompositorFrame (if it exists), the end of
      -- LatchToPresentation or another *ToPresentation substage such as
      -- SwapStartToPresentation (if either exists), or the end of
      -- LatchToSwapEnd (workaround in older Chrome versions).
      coalesce(
        max(iif(stage = 'SwapEndToPresentationCompositorFrame', end_ts, NULL)),
        max(iif(stage = 'LatchToPresentation', end_ts, NULL)),
        max(iif(stage = 'OtherToPresentation', end_ts, NULL)),
        max(iif(stage = 'LatchToSwapEnd', end_ts, NULL))
      ) AS presentation_timestamp
    FROM first_descendants_per_stage
    GROUP BY
      event_latency_id
  )
SELECT
  slice.id,
  slice.name,
  slice.ts,
  slice.dur,
  extract_arg(arg_set_id, 'event_latency.event_latency_id') AS scroll_update_id,
  extract_arg(arg_set_id, 'event_latency.surface_frame_trace_id') AS surface_frame_trace_id,
  extract_arg(arg_set_id, 'event_latency.display_trace_id') AS display_trace_id,
  coalesce(pivoted_stages.is_presented, 0) AS is_presented,
  extract_arg(arg_set_id, 'event_latency.event_type') AS event_type,
  slice.track_id,
  extract_arg(arg_set_id, 'event_latency.vsync_interval_ms') AS vsync_interval_ms,
  coalesce(extract_arg(arg_set_id, 'event_latency.is_janky_scrolled_frame'), 0) AS is_janky_scrolled_frame,
  pivoted_stages.buffer_available_timestamp,
  pivoted_stages.buffer_ready_timestamp,
  pivoted_stages.latch_timestamp,
  pivoted_stages.swap_end_timestamp,
  pivoted_stages.presentation_timestamp
FROM slice
LEFT JOIN pivoted_stages
  USING (id)
WHERE
  slice.name = 'EventLatency';

-- All scroll-related events (frames) including gesture scroll updates, begins
-- and ends with respective scroll ids and start/end timestamps, regardless of
-- being presented. This includes pinches that were presented. See b/315761896
-- for context on pinches.
CREATE PERFETTO TABLE chrome_gesture_scroll_updates (
  -- Slice Id for the EventLatency scroll event.
  id LONG,
  -- Slice name.
  name STRING,
  -- The start timestamp of the scroll.
  ts TIMESTAMP,
  -- The duration of the scroll.
  dur DURATION,
  -- The id of the scroll update event.
  scroll_update_id LONG,
  -- Whether this input event was presented.
  is_presented BOOL,
  -- EventLatency event type.
  event_type STRING,
  -- Perfetto track this slice is found on.
  track_id LONG,
  -- Vsync interval (in milliseconds).
  vsync_interval_ms DOUBLE,
  -- Whether the corresponding frame is janky based on the
  -- Event.ScrollJank.DelayedFramesPercentage.FixedWindow metric.
  is_janky BOOL,
  -- Timestamp of the BufferAvailableToBufferReady substage.
  buffer_available_timestamp LONG,
  -- Timestamp of the BufferReadyToLatch substage.
  buffer_ready_timestamp LONG,
  -- Timestamp of the LatchToSwapEnd substage (or LatchToPresentation as a
  -- fallback).
  latch_timestamp LONG,
  -- Timestamp of the SwapEndToPresentationCompositorFrame substage.
  swap_end_timestamp LONG,
  -- Frame presentation timestamp aka the timestamp of the
  -- SwapEndToPresentationCompositorFrame substage.
  -- TODO(b/341047059): temporarily use LatchToSwapEnd as a workaround if
  -- SwapEndToPresentationCompositorFrame is missing due to b/247542163.
  presentation_timestamp LONG,
  -- The id of the scroll.
  scroll_id LONG
) AS
-- To compute scroll id, we first mark all of the FIRST_GESTURE_SCROLL_UPDATE events
-- (or the first scroll update in the trace) as the points where scroll id should be
-- incremented and then use the cumulative sum as the scroll id.
WITH
  updates_without_scroll_ids AS (
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
      is_janky_scrolled_frame AS is_janky,
      buffer_available_timestamp,
      buffer_ready_timestamp,
      latch_timestamp,
      swap_end_timestamp,
      presentation_timestamp,
      (
        event_type = 'FIRST_GESTURE_SCROLL_UPDATE' OR row_number() OVER (ORDER BY ts) = 1
      ) AS is_first_update_in_scroll
    FROM chrome_event_latencies
    WHERE
      event_type IN ('GESTURE_SCROLL_UPDATE', 'FIRST_GESTURE_SCROLL_UPDATE', 'INERTIAL_GESTURE_SCROLL_UPDATE')
      OR (
        -- Pinches are only relevant if the frame was presented.
        event_type GLOB '*GESTURE_PINCH_UPDATE'
        AND is_presented
      )
  )
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
  is_janky,
  buffer_available_timestamp,
  buffer_ready_timestamp,
  latch_timestamp,
  swap_end_timestamp,
  presentation_timestamp,
  coalesce(
    sum(cast_int!(is_first_update_in_scroll)) OVER (ORDER BY ts ROWS BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW),
    0
  ) AS scroll_id
FROM updates_without_scroll_ids;
