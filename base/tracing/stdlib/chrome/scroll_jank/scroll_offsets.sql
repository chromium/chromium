-- Copyright 2023 The Chromium Authors
-- Use of this source code is governed by a BSD-style license that can be
-- found in the LICENSE file.

-- This file creates two public views:
--     - chrome_scroll_input_offsets and
--     - chrome_presented_scroll_offsets
--
-- These views store the pixel deltas and offsets for (respectively) all chrome
-- scroll inputs (coalesced and not coalesced), and for chrome presented frames
-- (not coalesced), along with the associated timestamp, and id.
--
-- Raw deltas are recorded as changes in pixel positions along the y-axis of a
-- screen, and are scaled to the viewport size. The corresponding trace event
-- for this is TranslateAndScaleWebInputEvent. These are the deltas for all
-- chrome scroll inputs.
--
-- For presented frames, the delta is calculated from the visual offset,
-- recorded once the input has been processed, in the
-- InputHandlerProxy::HandleGestureScrollUpdate_Result event. These values are
-- also scaled to the screen size.
--
-- Offsets are calculated by summing all of the deltas, ordered by timestamp.
-- For a given input/frame, the offset is the sum of its corresponding delta and
-- all previous deltas.
--
--
-- All values required for calculating deltas and offsets are recorded at
-- various stages of input processing, and are unified by a single
-- scroll_update_id value, recorded as scroll_deltas.trace_id in each event.

INCLUDE PERFETTO MODULE chrome.event_latency;
INCLUDE PERFETTO MODULE chrome.scroll_jank.scroll_jank_v3;

-- The raw input deltas for all input events which were part of a scroll.
CREATE PERFETTO TABLE chrome_scroll_input_deltas(
  -- Scroll update id (aka LatencyInfo.ID) for this scroll update input
  -- event.
  scroll_update_id LONG,
  -- The delta in pixels (scaled to the device's screen size) how much this
  -- input event moved over the X axis vs previous, as reported by the OS.
  delta_x DOUBLE,
  -- The delta in pixels (scaled to the device's screen size) how much this
  -- input event moved over the Y axis vs previous, as reported by the OS.
  delta_y DOUBLE
) AS
SELECT
  EXTRACT_ARG(arg_set_id, 'scroll_deltas.trace_id') AS scroll_update_id,
  EXTRACT_ARG(arg_set_id, 'scroll_deltas.original_delta_x') AS delta_x,
  EXTRACT_ARG(arg_set_id, 'scroll_deltas.original_delta_y') AS delta_y
FROM slice
WHERE slice.name = 'TranslateAndScaleWebInputEvent';

-- The raw coordinates and pixel offsets for all input events which were part of
-- a scroll.
CREATE PERFETTO TABLE chrome_scroll_input_offsets(
  -- An ID for this scroll update (aka LatencyInfo.ID).
  scroll_update_id LONG,
  -- An ID for the scroll this scroll update belongs to.
  scroll_id LONG,
  -- Timestamp the of the scroll input event.
  ts TIMESTAMP,
  -- The delta in raw coordinates between this scroll update event and the
  -- previous.
  delta_y DOUBLE,
  -- The total delta of all scroll updates within the same as scroll up to and
  -- including this scroll update.
  relative_offset_y DOUBLE
) AS
SELECT
  delta.scroll_update_id,
  scroll_update.scroll_id,
  ts,
  delta_y,
  SUM(delta_y) OVER (
    PARTITION BY scroll_id
    ORDER BY ts
    ROWS BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW
  ) AS relative_offset_y
FROM chrome_scroll_input_deltas delta
JOIN chrome_gesture_scroll_updates scroll_update USING (scroll_update_id);

-- The page offset delta (by how much the page was scrolled vs previous frame)
-- for each frame.
-- This is the resulting delta that is shown to the user after the input has
-- been processed. `chrome_scroll_input_deltas` tracks the underlying signal
-- deltas between consecutive input events.
CREATE PERFETTO TABLE chrome_scroll_presented_deltas(
  -- Scroll update id (aka LatencyInfo.ID) for this scroll update input
  -- event.
  scroll_update_id LONG,
  -- The delta in pixels (scaled to the device's screen size) how much this
  -- input event moved over the X axis vs previous, as reported by the OS.
  delta_x DOUBLE,
  -- The delta in pixels (scaled to the device's screen size) how much this
  -- input event moved over the Y axis vs previous, as reported by the OS.
  delta_y DOUBLE,
  -- The page offset in pixels (scaled to the device's screen size) along
  -- the X axis.
  offset_x LONG,
  -- The page offset in pixels (scaled to the device's screen size) along
  -- the Y axis.
  offset_y LONG
) AS
SELECT
  EXTRACT_ARG(arg_set_id, 'scroll_deltas.trace_id') AS scroll_update_id,
  EXTRACT_ARG(arg_set_id, 'scroll_deltas.provided_to_compositor_delta_x') AS delta_x,
  EXTRACT_ARG(arg_set_id, 'scroll_deltas.provided_to_compositor_delta_y') AS delta_y,
  EXTRACT_ARG(arg_set_id, 'scroll_deltas.visual_offset_x') AS offset_x,
  EXTRACT_ARG(arg_set_id, 'scroll_deltas.visual_offset_y') AS offset_y
FROM slice
WHERE slice.name = 'InputHandlerProxy::HandleGestureScrollUpdate_Result';

-- Associate the gesture scroll update OS timestamp with the delta.
CREATE PERFETTO TABLE _scroll_deltas_with_timestamp AS
SELECT
  slice.id AS event_latency_slice_id,
  slice.ts AS input_ts,
  data.scroll_update_id,
  data.delta_y
FROM chrome_scroll_input_deltas data
  JOIN slice ON slice.name = 'EventLatency'
    AND data.scroll_update_id = EXTRACT_ARG(arg_set_id,
        'event_latency.event_latency_id');

-- Associate the scroll update/delta with the correct scroll.
CREATE PERFETTO TABLE _scroll_deltas_with_scroll_id AS
SELECT
  scrolls.id AS scroll_id,
  deltas.event_latency_slice_id,
  deltas.input_ts,
  deltas.scroll_update_id,
  deltas.delta_y
FROM _scroll_deltas_with_timestamp deltas
  LEFT JOIN chrome_scrolls scrolls
    ON deltas.input_ts >= scrolls.ts
      AND deltas.input_ts <= scrolls.ts + scrolls.dur;

-- Associate the presentation timestamp/deltas with the user deltas.
CREATE PERFETTO TABLE _scroll_deltas_with_delays AS
SELECT
  deltas.scroll_id,
  delay.total_delta,
  deltas.scroll_update_id,
  deltas.event_latency_slice_id,
  delay.presentation_timestamp AS presentation_timestamp,
  deltas.input_ts,
  deltas.delta_y
FROM _scroll_deltas_with_scroll_id AS deltas
  LEFT JOIN chrome_frame_info_with_delay AS delay USING(scroll_update_id);

-- The scrolling offsets for the actual (applied) scroll events. These are not
-- necessarily inclusive of all user scroll events, rather those scroll events
-- that are actually processed.
CREATE PERFETTO TABLE chrome_presented_scroll_offsets(
  -- An ID for this scroll update (aka LatencyInfo.ID).
  scroll_update_id LONG,
  -- An ID for the scroll this scroll update belongs to.
  scroll_id LONG,
  -- Presentation timestamp.
  ts TIMESTAMP,
  -- The delta in raw coordinates between this scroll update event and the
  -- previous.
  delta_y DOUBLE,
  -- The pixel offset of this scroll update event compared to the initial one.
  relative_offset_y DOUBLE
) AS
WITH data AS (
  SELECT
    scroll_update_id,
    scroll_id,
    presentation_timestamp AS ts,
    -- Aggregate the deltas for each presentation time.
    SUM(delta_y) OVER (PARTITION BY presentation_timestamp) AS delta_y,
    SUM(delta_y) OVER (
      PARTITION BY scroll_id
      ORDER BY presentation_timestamp
      GROUPS BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW
    ) AS relative_offset_y,
    -- For each presentation time, select the last scroll update as there can
    -- be multiple EventLatencies with the same presentation time.
    ROW_NUMBER() OVER (
        PARTITION BY presentation_timestamp
        ORDER BY scroll_update.ts
    ) AS rank
  FROM chrome_scroll_presented_deltas
  JOIN chrome_gesture_scroll_updates scroll_update USING (scroll_update_id)
)
SELECT
  scroll_update_id,
  scroll_id,
  ts,
  delta_y,
  relative_offset_y
FROM data
WHERE rank = 1;