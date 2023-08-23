-- Copyright 2023 The Chromium Authors
-- Use of this source code is governed by a BSD-style license that can be
-- found in the LICENSE file.
--
-- A scroll jank metric based on EventLatency slices.
--
-- We define an update to be janky if comparing forwards or backwards (ignoring
-- coalesced and not shown on the screen updates) a given updates exceeds the duration
-- of its predecessor or successor by 50% of a vsync interval (defaulted to 60 FPS).
--
-- WARNING: This metric should not be used as a source of truth. It is under
--          active development and the values & meaning might change without
--          notice.

SELECT IMPORT('chrome.scroll_jank.utils');
SELECT IMPORT('chrome.scroll_jank.event_latency_to_breakdowns');
SELECT IMPORT('chrome.vsync_intervals');

-- Creates table view where each EventLatency event has its upid.
CREATE VIEW internal_event_latency_with_track
AS
SELECT
  slice.*,
  process_track.upid AS upid
FROM slice JOIN process_track
  ON slice.track_id = process_track.id
WHERE slice.name = "EventLatency";

-- Select scroll EventLatency events that were shown on the screen.
-- An update event was shown on the screen if and only if
-- it has a "SubmitCompositorFrameToPresentationCompositorFrame" breakdown.
-- But this logic is not applied for begin events, because a begin event is an artificial marker
-- and never gets shown to the screen because it doesn't contain any update.
-- Also it automatically only includes non-coalesced EventLatency events,
-- because coalesced ones are not shown on the screen.
CREATE VIEW internal_filtered_scroll_event_latency
AS
WITH shown_on_display_event_latency_ids AS (
  SELECT
  event_latency_id
  FROM chrome_event_latency_breakdowns
  WHERE name = "SubmitCompositorFrameToPresentationCompositorFrame" OR event_type = "GESTURE_SCROLL_BEGIN"
)
SELECT
  internal_event_latency_with_track.id,
  internal_event_latency_with_track.track_id,
  internal_event_latency_with_track.upid,
  internal_event_latency_with_track.ts,
  internal_event_latency_with_track.dur,
  EXTRACT_ARG(internal_event_latency_with_track.arg_set_id, "event_latency.event_type") AS event_type
FROM internal_event_latency_with_track JOIN shown_on_display_event_latency_ids
  ON internal_event_latency_with_track.id = shown_on_display_event_latency_ids.event_latency_id
WHERE
  event_type IN (
    "GESTURE_SCROLL_BEGIN", "GESTURE_SCROLL_UPDATE",
    "INERTIAL_GESTURE_SCROLL_UPDATE", "FIRST_GESTURE_SCROLL_UPDATE");

-- Select begin events and it's next begin event within the same process (same upid).
--
-- Note: Must be a TABLE because it uses a window function which can behave
--       strangely in views.
CREATE PERFETTO TABLE internal_scroll_event_latency_begins
AS
SELECT
  *,
  LEAD(ts) OVER sorted_begins AS next_gesture_begin_ts
FROM internal_filtered_scroll_event_latency
WHERE event_type = "GESTURE_SCROLL_BEGIN"
WINDOW sorted_begins AS (PARTITION BY upid ORDER BY ts ASC);

-- For each scroll update event finds it's begin event.
-- Pair [upid, next_gesture_begin_ts] represent a gesture key.
-- We need to know the gesture key of gesture scroll to calculate a jank only within this gesture scroll.
-- Because different gesture scrolls can have different properties.
CREATE VIEW internal_scroll_event_latency_updates
AS
SELECT
  internal_filtered_scroll_event_latency.*,
  internal_scroll_event_latency_begins.ts AS gesture_begin_ts,
  internal_scroll_event_latency_begins.next_gesture_begin_ts AS next_gesture_begin_ts
FROM internal_filtered_scroll_event_latency LEFT JOIN internal_scroll_event_latency_begins
  ON internal_filtered_scroll_event_latency.ts >= internal_scroll_event_latency_begins.ts
     AND (internal_filtered_scroll_event_latency.ts < next_gesture_begin_ts OR next_gesture_begin_ts IS NULL)
     AND internal_filtered_scroll_event_latency.upid = internal_scroll_event_latency_begins.upid
WHERE internal_filtered_scroll_event_latency.id != internal_scroll_event_latency_begins.id
      AND internal_filtered_scroll_event_latency.event_type != "GESTURE_SCROLL_BEGIN";

-- Find the last EventLatency scroll update event in the scroll.
-- We will use the last EventLatency event instead of "InputLatency::GestureScrollEnd" event.
-- We need to know when the scroll gesture ends so that we can later calculate
-- the average vsync interval just up to the end of the gesture.
CREATE VIEW internal_scroll_event_latency_updates_ends
AS
SELECT
  id,
  upid,
  gesture_begin_ts,
  ts,
  dur,
  MAX(ts + dur) AS gesture_end_ts
FROM internal_scroll_event_latency_updates
GROUP BY upid, gesture_begin_ts;

CREATE VIEW internal_scroll_event_latency_updates_with_ends
AS
SELECT
  internal_scroll_event_latency_updates.*,
  internal_scroll_event_latency_updates_ends.gesture_end_ts AS gesture_end_ts
FROM internal_scroll_event_latency_updates LEFT JOIN internal_scroll_event_latency_updates_ends
  ON internal_scroll_event_latency_updates.upid = internal_scroll_event_latency_updates_ends.upid
    AND internal_scroll_event_latency_updates.gesture_begin_ts = internal_scroll_event_latency_updates_ends.gesture_begin_ts;

-- Creates table where each event contains info about it's previous and next events.
-- We consider only previous and next events from the same scroll id
-- to don't calculate a jank between different scrolls.
--
-- Note: Must be a TABLE because it uses a window function which can behave
--       strangely in views.
CREATE PERFETTO TABLE internal_scroll_event_latency_with_neighbours
AS
SELECT
  *,
  LEAD(id) OVER sorted_events AS next_id,
  LEAD(ts) OVER sorted_events AS next_ts,
  LEAD(dur) OVER sorted_events AS next_dur,
  LAG(id) OVER sorted_events AS prev_id,
  LAG(ts) OVER sorted_events AS prev_ts,
  LAG(dur) OVER sorted_events AS prev_dur,
  calculate_avg_vsync_interval(gesture_begin_ts, gesture_end_ts) AS avg_vsync_interval
FROM internal_scroll_event_latency_updates_with_ends
WINDOW sorted_events AS (PARTITION BY upid, next_gesture_begin_ts ORDER BY id ASC, ts ASC);

CREATE VIEW internal_scroll_event_latency_neighbors_jank
AS
SELECT
  is_janky_frame(gesture_begin_ts, gesture_begin_ts, next_ts,
    gesture_begin_ts, gesture_end_ts, dur / avg_vsync_interval, next_dur / avg_vsync_interval) AS next_jank,
  is_janky_frame(gesture_begin_ts, gesture_begin_ts, prev_ts,
    gesture_begin_ts, gesture_end_ts, dur / avg_vsync_interval, prev_dur / avg_vsync_interval) AS prev_jank,
  internal_scroll_event_latency_with_neighbours.*
FROM internal_scroll_event_latency_with_neighbours;

-- Creates a view where each event contains information about whether it is janky
-- with respect to previous and next events within the same scroll.
--
-- @column jank                   Whether this event is janky.
-- @column next_jank              Whether the subsequent event is janky.
-- @column prev_jank              Whether the previous event is janky.
-- @column id                     ID of the frame.
-- @column track_id               Track ID of the frame.
-- @column upid                   Process ID of the frame.
-- @column ts                     Timestamp of the frame.
-- @column dur                    Duration of the frame.
-- @column event_type             Event type.
-- @column gesture_begin_ts       Timestamp of the associated gesture begin
--                                event.
-- @column next_gesture_begin_ts  Timestamp of the subsequent gesture begin
--                                event.
-- @column gesture_end_ts         Timestamp of the associated gesture end event.
-- @column next_id                ID of the next frame
-- @column next_ts                Timestamp of the next frame.
-- @column next_dur               Duration of the next frame.
-- @column prev_id                ID of the previous frame.
-- @column prev_ts                Timestamp of the previous frame.
-- @column prev_dur               Duration of the previous frame.
CREATE VIEW chrome_scroll_event_latency_jank
AS
SELECT
  (next_jank IS NOT NULL AND next_jank) OR (prev_jank IS NOT NULL AND prev_jank) AS jank,
  *
FROM internal_scroll_event_latency_neighbors_jank;
