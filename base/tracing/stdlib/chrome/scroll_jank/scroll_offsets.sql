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

INCLUDE PERFETTO MODULE chrome.chrome_scrolls;
INCLUDE PERFETTO MODULE chrome.scroll_jank.scroll_jank_v3;

-- All (coalesced and non-coalesced) vertical scrolling deltas and their
-- associated scroll ids. Delta values are recorded after being scaled to the
-- device's screen size in the TranslateAndScaleWebInputEvent trace event. In
-- this trace event, the deltas recorded represent the true (read "original")
-- values that the Browser receives from Android, and the only processing is
-- scaling and translation.
CREATE PERFETTO TABLE _translate_and_scale_scroll_deltas AS
SELECT
  EXTRACT_ARG(arg_set_id, 'scroll_deltas.trace_id') AS scroll_update_id,
  EXTRACT_ARG(arg_set_id, 'scroll_deltas.original_delta_y') AS delta_y
FROM slice
WHERE slice.name = 'TranslateAndScaleWebInputEvent';

-- Associate the gesture scroll update OS timestamp with the delta.
CREATE PERFETTO TABLE _scroll_deltas_with_timestamp AS
SELECT
  slice.id AS event_latency_slice_id,
  slice.ts AS input_ts,
  data.scroll_update_id,
  data.delta_y
FROM _translate_and_scale_scroll_deltas data
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

-- The raw coordinates and pixel offsets for all input events which were part of
-- a scroll.
CREATE PERFETTO TABLE chrome_scroll_input_offsets(
  -- An ID that ties all EventLatencies in a particular scroll. (implementation
  -- note: This is the EventLatency TraceId of the GestureScrollbegin).
  scroll_id INT,
  -- An ID for this particular EventLatency regardless of it being presented or
  -- not.
  event_latency_slice_id INT,
  -- An ID that ties this |event_latency_id| with the Trace Id (another
  -- event_latency_id) that it was presented with.
  scroll_update_id INT,
  -- Timestamp the of the scroll input event.
  ts INT,
  -- The delta in raw coordinates between this scroll update event and the
  -- previous.
  delta_y DOUBLE,
  -- The pixel offset of this scroll update event compared to the initial one.
  relative_offset_y DOUBLE
) AS
SELECT
  scroll_id,
  event_latency_slice_id,
  scroll_update_id,
  input_ts AS ts,
  delta_y,
  SUM(IFNULL(delta_y, 0)) OVER ( PARTITION BY scroll_id
    ORDER BY scroll_update_id, input_ts
    ROWS BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW) AS relative_offset_y
FROM _scroll_deltas_with_delays;

-- The scrolling offsets for the actual (applied) scroll events. These are not
-- necessarily inclusive of all user scroll events, rather those scroll events
-- that are actually processed.
CREATE PERFETTO TABLE chrome_presented_scroll_offsets(
  -- An ID that ties all EventLatencies in a particular scroll. (implementation
  -- note: This is the EventLatency TraceId of the GestureScrollbegin).
  scroll_id INT,
  -- An ID for this particular EventLatency regardless of it being presented or
  -- not.
  event_latency_slice_id INT,
  -- An ID that ties this |event_latency_id| with the Trace Id (another
  -- event_latency_id) that it was presented with.
  scroll_update_id INT,
  -- Presentation timestamp.
  ts INT,
  -- The delta in raw coordinates between this scroll update event and the
  -- previous.
  delta_y DOUBLE,
  -- The pixel offset of this scroll update event compared to the initial one.
  relative_offset_y DOUBLE
) AS
SELECT
  scroll_id,
  event_latency_slice_id,
  scroll_update_id,
  presentation_timestamp AS ts,
  total_delta AS delta_y,
  SUM(IFNULL(total_delta, 0)) OVER ( PARTITION BY scroll_id
    ORDER BY scroll_update_id, presentation_timestamp
    ROWS BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW) AS relative_offset_y
FROM _scroll_deltas_with_delays
WHERE presentation_timestamp IS NOT NULL
;