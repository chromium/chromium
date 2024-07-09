-- Copyright 2024 The Chromium Authors
-- Use of this source code is governed by a BSD-style license that can be
-- found in the LICENSE file.

INCLUDE PERFETTO MODULE deprecated.v42.common.slices;

-- Finds the end timestamp for a given slice's descendant with a given name.
-- If there are multiple descendants with a given name, the function will return
-- the first one, so it's most useful when working with a timeline broken down
-- into phases, where each subphase can happen only once.
CREATE PERFETTO FUNCTION _descendant_slice_end(
  -- Id of the parent slice.
  parent_id INT,
  -- Name of the child with the desired end TS.
  child_name STRING
)
-- End timestamp of the child or NULL if it doesn't exist.
RETURNS INT AS
SELECT
  CASE WHEN s.dur
    IS NOT -1 THEN s.ts + s.dur
    ELSE NULL
  END
FROM descendant_slice($parent_id) s
WHERE s.name GLOB $child_name
LIMIT 1;

-- Returns the presentation timestamp for a given EventLatency slice.
-- This is either the end of
-- SwapEndToPresentationCompositorFrame (if it exists),
-- the end of LatchToPresentation (if it exists),
-- the end of SwapStartToPresentation (if it exists),
-- or the end of LatchToSwapEnd (workaround in older Chrome versions).
CREATE PERFETTO FUNCTION _get_presentation_timestamp(
  -- The slice id which we need the presentation timestamp for.
  id LONG
)
RETURNS INT AS
SELECT
  COALESCE(_descendant_slice_end(id, 'SwapEndToPresentationCompositorFrame'),
    _descendant_slice_end(id, '*ToPresentation'),
    _descendant_slice_end(id, 'LatchToSwapEnd'))
FROM slice WHERE $id = id;

-- All EventLatency slices.
CREATE PERFETTO TABLE chrome_event_latencies(
  -- Slice Id for the EventLatency scroll event.
  id INT,
  -- Slice name.
  name STRING,
  -- The start timestamp of the scroll.
  ts INT,
  -- The duration of the scroll.
  dur INT,
  -- The id of the scroll update event.
  scroll_update_id INT,
  -- Whether this input event was presented.
  is_presented BOOL,
  -- EventLatency event type.
  event_type STRING,
  -- Perfetto track this slice is found on.
  track_id INT
) AS
SELECT
  slice.id,
  slice.name,
  slice.ts,
  slice.dur,
  EXTRACT_arg(arg_set_id, 'event_latency.event_latency_id') AS scroll_update_id,
  has_descendant_slice_with_name(
    slice.id,
    'SubmitCompositorFrameToPresentationCompositorFrame')
  AS is_presented,
  EXTRACT_ARG(arg_set_id, 'event_latency.event_type') AS event_type,
  slice.track_id
FROM slice
WHERE name = 'EventLatency';

-- All EventLatency slices that are relevant to scrolling, including presented
-- pinches. Materialized to reduce how many times we query slice.
CREATE PERFETTO TABLE _gesture_scroll_events_no_scroll_id
AS
SELECT
  name,
  ts,
  dur,
  id,
  scroll_update_id,
  is_presented,
  _get_presentation_timestamp(chrome_event_latencies.id)
  AS presentation_timestamp,
  event_type,
  track_id
FROM chrome_event_latencies
WHERE (
  event_type GLOB '*GESTURE_SCROLL*'
  -- Pinches are only relevant if the frame was presented.
  OR (event_type GLOB '*GESTURE_PINCH_UPDATE'
    AND has_descendant_slice_with_name(
      id,
      'SubmitCompositorFrameToPresentationCompositorFrame')
  )
);

-- Extracts scroll id for the EventLatency slice at `ts`.
CREATE PERFETTO FUNCTION chrome_get_most_recent_scroll_begin_id(
  -- Timestamp of the EventLatency slice to get the scroll id for.
  ts INT)
-- The event_latency_id of the EventLatency slice with the type
-- GESTURE_SCROLL_BEGIN that is the closest to `ts`.
RETURNS INT AS
SELECT scroll_update_id
FROM _gesture_scroll_events_no_scroll_id
WHERE event_type = 'GESTURE_SCROLL_BEGIN'
AND ts<=$ts
ORDER BY ts DESC
LIMIT 1;

-- All scroll-related events (frames) including gesture scroll updates, begins
-- and ends with respective scroll ids and start/end timestamps, regardless of
-- being presented. This includes pinches that were presented. See b/315761896
-- for context on pinches.
CREATE PERFETTO TABLE chrome_gesture_scroll_events(
  -- Slice Id for the EventLatency scroll event.
  id INT,
  -- Slice name.
  name STRING,
  -- The start timestamp of the scroll.
  ts INT,
  -- The duration of the scroll.
  dur INT,
  -- The id of the scroll update event.
  scroll_update_id INT,
  -- The id of the scroll.
  scroll_id INT,
  -- Whether this input event was presented.
  is_presented BOOL,
  -- Frame presentation timestamp aka the timestamp of the
  -- SwapEndToPresentationCompositorFrame substage.
  -- TODO(b/341047059): temporarily use LatchToSwapEnd as a workaround if
  -- SwapEndToPresentationCompositorFrame is missing due to b/247542163.
  presentation_timestamp INT,
  -- EventLatency event type.
  event_type STRING,
  -- Perfetto track this slice is found on.
  track_id INT
) AS
SELECT
  id,
  name,
  ts,
  dur,
  scroll_update_id,
  chrome_get_most_recent_scroll_begin_id(ts) AS scroll_id,
  is_presented,
  presentation_timestamp,
  event_type,
  track_id
FROM _gesture_scroll_events_no_scroll_id;
