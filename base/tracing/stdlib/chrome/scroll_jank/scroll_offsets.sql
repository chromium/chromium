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

INCLUDE PERFETTO MODULE chrome.scroll_jank.scroll_jank_v3;

-- Non-coalesced scroll update events and their timestamps.
CREATE PERFETTO VIEW internal_non_coalesced_scrolls AS
SELECT
  scroll_update_id,
  ts
FROM chrome_gesture_scroll_updates
WHERE is_coalesced = False;

-- All (coalesced and non-coalesced) vertical scrolling deltas and their
-- associated scroll ids. Delta values are recorded after being scaled to the
-- device's screen size in the TranslateAndScaleWebInputEvent trace event. In
-- this trace event, the deltas recorded represent the true (read "original")
-- values that the Browser receives from Android, and the only processing is
-- scaling and translation.
CREATE PERFETTO TABLE internal_scroll_deltas AS
SELECT
  EXTRACT_ARG(arg_set_id, 'scroll_deltas.trace_id') AS scroll_update_id,
  EXTRACT_ARG(arg_set_id, 'scroll_deltas.original_delta_y') AS delta_y,
  EXTRACT_ARG(arg_set_id, 'scroll_deltas.original_delta_y') IS NOT NULL AS is_coalesced
FROM slice
WHERE name = "TranslateAndScaleWebInputEvent";

-- Associate the raw (original) deltas (internal_scroll_deltas) with the
-- corresponding non-coalesced scroll updates
-- (internal_non_coalesced_scroll_updates) to get the timestamp of the event
-- those deltas. This allows for ordering delta recordings to track them over
-- time.
CREATE PERFETTO VIEW internal_non_coalesced_deltas AS
SELECT
  scroll_update_id,
  ts,
  delta_y
FROM internal_non_coalesced_scrolls
INNER JOIN internal_scroll_deltas
  USING (scroll_update_id);

-- Selecting information scroll update events that have been coalesced,
-- including timestamp and the specific event (scroll update id) it was
-- coalesced into. Recordings of deltas will need to be associated with the
-- timestamp of the scroll update they were coalesced into.
CREATE PERFETTO TABLE internal_scroll_update_coalesce_info AS
SELECT
  ts,
  EXTRACT_ARG(arg_set_id, 'scroll_deltas.coalesced_to_trace_id') AS coalesced_to_scroll_update_id,
  EXTRACT_ARG(arg_set_id, 'scroll_deltas.trace_id') AS scroll_update_id
FROM slice
WHERE name = "WebCoalescedInputEvent::CoalesceWith" AND
  coalesced_to_scroll_update_id IS NOT NULL;

-- Associate the raw (original) deltas (internal_scroll_deltas) with the
-- corresponding coalesced scroll updates (internal_scroll_update_coalesce_info)
-- to get the timestamp of the event those deltas were coalesced into. This
-- allows us to get the scaled coordinates for all of the input events
-- (original input coordinates can't be used due to scaling).
CREATE PERFETTO VIEW internal_coalesced_deltas AS
SELECT
  internal_scroll_update_coalesce_info.coalesced_to_scroll_update_id AS scroll_update_id,
  ts,
  internal_scroll_deltas.delta_y AS delta_y,
  TRUE AS is_coalesced
FROM internal_scroll_update_coalesce_info
LEFT JOIN internal_scroll_deltas
  USING (scroll_update_id);

-- All of the presented frame scroll update ids.
CREATE PERFETTO VIEW chrome_deltas_presented_frame_scroll_update_ids(
  -- A scroll update id that was included in the presented frame.
  -- There may be zero, one, or more.
  scroll_update_id INT,
  -- Slice id
  id INT
) AS
SELECT
  args.int_value AS scroll_update_id,
  slice.id
FROM args
LEFT JOIN slice
  USING (arg_set_id)
WHERE slice.name = 'PresentedFrameInformation'
AND args.flat_key GLOB 'scroll_deltas.trace_ids_in_gpu_frame*';;

-- When every GestureScrollUpdate event is processed, the offset set by the
-- compositor is recorded. This offset is scaled to the device screen size, and
-- can be used to calculate deltas.
CREATE PERFETTO VIEW internal_presented_frame_offsets AS
SELECT
  EXTRACT_ARG(arg_set_id, 'scroll_deltas.trace_id') AS scroll_update_id,
  EXTRACT_ARG(arg_set_id, 'scroll_deltas.visual_offset_y') AS visual_offset_y
FROM slice
WHERE name = 'InputHandlerProxy::HandleGestureScrollUpdate_Result';

-- The raw coordinates and pixel offsets for all input events which were part of
-- a scroll. This includes input events that were converted to scroll events
-- which were presented (internal_non_coalesced_scrolls) and scroll events which
-- were coalesced (internal_coalesced_deltas).
CREATE PERFETTO TABLE chrome_scroll_input_offsets(
  -- Trace id associated with the scroll.
  scroll_update_id INT,
  -- Timestamp the of the scroll input event.
  ts INT,
  -- The delta in raw coordinates between this scroll update event and the previous.
  delta_y INT,
  -- The pixel offset of this scroll update event compared to the previous one.
  offset_y INT
) AS
-- First collect all coalesced and non-coalesced deltas so that the offsets
-- can be calculated from them in order of timestamp.
WITH all_deltas AS (
  SELECT
    scroll_update_id,
    ts,
    delta_y
  FROM internal_non_coalesced_deltas
  WHERE delta_y IS NOT NULL
  UNION
  SELECT
    scroll_update_id,
    ts,
    delta_y
  FROM internal_coalesced_deltas
  WHERE delta_y IS NOT NULL
  ORDER BY scroll_update_id, ts)
SELECT
  scroll_update_id,
  ts,
  delta_y,
  SUM(IFNULL(delta_y, 0)) OVER (
    ORDER BY scroll_update_id, ts
    ROWS BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW) AS offset_y
FROM all_deltas;

-- Calculate the total visual offset for all presented frames (non-coalesced
-- scroll updates) that have raw deltas recorded. These visual offsets
-- correspond with the inverse of the deltas for the presented frame.
CREATE PERFETTO VIEW internal_preprocessed_presented_frame_offsets AS
SELECT
  chrome_full_frame_view.scroll_update_id,
  chrome_full_frame_view.presentation_timestamp AS ts,
  chrome_deltas_presented_frame_scroll_update_ids.id,
  internal_presented_frame_offsets.visual_offset_y -
    LAG(internal_presented_frame_offsets.visual_offset_y)
    OVER (ORDER BY chrome_full_frame_view.presentation_timestamp)
      AS presented_frame_visual_offset_y
FROM chrome_full_frame_view
LEFT JOIN internal_scroll_deltas
  USING (scroll_update_id)
LEFT JOIN chrome_deltas_presented_frame_scroll_update_ids
  USING (scroll_update_id)
LEFT JOIN internal_presented_frame_offsets
  USING (scroll_update_id)
WHERE internal_scroll_deltas.delta_y IS NOT NULL;

-- The scrolling offsets for the actual (applied) scroll events. These are not
-- necessarily inclusive of all user scroll events, rather those scroll events
-- that are actually processed.
CREATE PERFETTO TABLE chrome_presented_scroll_offsets(
  -- Trace Id associated with the scroll.
  scroll_update_id INT,
  -- Presentation timestamp.
  ts INT,
  -- The delta in coordinates as processed by Chrome between this scroll update
  -- event and the previous.
  delta_y INT,
  -- The pixel offset of this scroll update (the presented frame) compared to
  -- the previous one.
  offset_y INT
) AS
WITH all_deltas AS (
  SELECT
    scroll_update_id,
    id,
    MAX(ts) AS ts,
    SUM(presented_frame_visual_offset_y) * -1 AS delta_y
  FROM internal_preprocessed_presented_frame_offsets
  GROUP BY id
  ORDER BY ts)
SELECT
  scroll_update_id,
  ts,
  delta_y,
  SUM(IFNULL(delta_y, 0)) OVER (
    ORDER BY scroll_update_id, ts
    ROWS BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW) AS offset_y
FROM all_deltas;
