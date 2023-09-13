-- Copyright 2023 The Chromium Authors
-- Use of this source code is governed by a BSD-style license that can be
-- found in the LICENSE file.
--
-- Calculating how often each breakdown causes a scroll jank.
-- We define that a breakdown causes a scroll jank in the janky EventLatency event if it increased
-- more than other breakdowns relatively to its neighbor EventLatency events.
--
-- WARNING: This metric should not be used as a source of truth. It is under
--          active development and the values & meaning might change without
--          notice.


IMPORT PERFETTO MODULE chrome.scroll_jank.event_latency_scroll_jank;

-- Calculating the jank delta for EventLatency events which are janky relatively to its next EventLatency event.
-- For breakdowns that exist in the current EventLatency but not the next EventLatency
-- we use a default of 0 so that the full duration is considered when looking for the maximum increase.
-- Breakdowns that exist in the next EventLatency event, but not in the current EventLatency event,
-- are ignored because they do not cause a jank anyway.
CREATE PERFETTO TABLE internal_event_latency_scroll_breakdowns_next_jank_deltas
AS
SELECT
    cur_breakdowns.*,
    next_breakdowns.event_type as next_event_type,
    next_breakdowns.slice_id as next_breakdown_id,
    next_breakdowns.dur as next_dur,
    cur_breakdowns.dur - COALESCE(next_breakdowns.dur, 0) as delta_dur_ns
FROM internal_event_latency_scroll_breakdowns_jank as cur_breakdowns LEFT JOIN internal_event_latency_scroll_breakdowns_jank as next_breakdowns
ON cur_breakdowns.next_event_latency_id = next_breakdowns.event_latency_id AND
    cur_breakdowns.name = next_breakdowns.name
WHERE cur_breakdowns.next_jank = 1;

-- Calculating the jank delta for EventLatency events which are janky relatively to its prev EventLatency event.
-- For breakdowns that exist in the current EventLatency but not the prev EventLatency
-- we use a default of 0 so that the full duration is considered when looking for the maximum increase.
-- Breakdowns that exist in the prev EventLatency event, but not in the current EventLatency event,
-- are ignored because they do not cause a jank anyway.
CREATE VIEW internal_event_latency_scroll_breakdowns_prev_jank_deltas
AS
SELECT
    cur_breakdowns.*,
    prev_breakdowns.event_type as prev_event_type,
    prev_breakdowns.slice_id as prev_breakdown_id,
    prev_breakdowns.dur as prev_dur,
    cur_breakdowns.dur - COALESCE(prev_breakdowns.dur, 0) as delta_dur_ns
FROM internal_event_latency_scroll_breakdowns_jank as cur_breakdowns LEFT JOIN internal_event_latency_scroll_breakdowns_jank as prev_breakdowns
ON cur_breakdowns.prev_event_latency_id = prev_breakdowns.event_latency_id AND
    cur_breakdowns.name = prev_breakdowns.name
WHERE cur_breakdowns.prev_jank = 1;

-- Add a jank indicator to each breakdown. Jank indicator is related to an entire EventLatency envent, not only to a breakdown.
CREATE VIEW internal_event_latency_scroll_breakdowns_jank
AS
SELECT
  chrome_event_latency_breakdowns.*,
  chrome_scroll_event_latency_jank.jank,
  chrome_scroll_event_latency_jank.next_jank,
  chrome_scroll_event_latency_jank.prev_jank,
  chrome_scroll_event_latency_jank.next_id as next_event_latency_id,
  chrome_scroll_event_latency_jank.prev_id as prev_event_latency_id
FROM chrome_event_latency_breakdowns JOIN chrome_scroll_event_latency_jank
ON chrome_event_latency_breakdowns.event_latency_id = chrome_scroll_event_latency_jank.id
WHERE chrome_event_latency_breakdowns.event_type in ("GESTURE_SCROLL_UPDATE", "FIRST_GESTURE_SCROLL_UPDATE", "INERTIAL_GESTURE_SCROLL_UPDATE");

-- Merge breakdowns from the |internal_event_latency_scroll_breakdowns_next_jank_deltas|
-- and |internal_event_latency_scroll_breakdowns_prev_jank_deltas| tables and select the maximum |delta_dur_ns| of them.
-- This is necessary in order to get a single reason for the jank for the event later.
CREATE VIEW internal_event_latency_scroll_breakdowns_max_jank_deltas
AS
SELECT
  COALESCE(next.slice_id, prev.slice_id) as slice_id,
  COALESCE(next.name, prev.name) as name,
  COALESCE(next.event_latency_id, prev.event_latency_id) as event_latency_id,
  COALESCE(next.event_latency_track_id, prev.event_latency_track_id) as track_id,
  COALESCE(next.event_latency_dur, prev.event_latency_dur) as event_latency_dur,
  COALESCE(next.event_latency_ts, prev.event_latency_ts) as event_latency_ts,
  COALESCE(next.event_type, prev.event_type) as event_type,
  COALESCE(next.event_latency_ts, prev.event_latency_ts) as ts,
  COALESCE(next.event_latency_dur, prev.event_latency_dur) as dur,
  COALESCE(next.jank, prev.jank) as jank,
  COALESCE(next.next_jank, 0) as next_jank,
  COALESCE(prev.prev_jank, 0) as prev_jank,
  next.next_event_latency_id as next_event_latency_id,
  prev.prev_event_latency_id as prev_event_latency_id,
  next.delta_dur_ns as next_delta_dur_ns,
  prev.delta_dur_ns as prev_delta_dur_ns,
  CASE
    WHEN prev.delta_dur_ns IS NULL OR next.delta_dur_ns >  prev.delta_dur_ns
      THEN next.delta_dur_ns
    ELSE prev.delta_dur_ns
  END as delta_dur_ns,
  CASE
    WHEN prev.delta_dur_ns IS NULL OR next.delta_dur_ns >  prev.delta_dur_ns
      THEN next.next_breakdown_id
    ELSE prev.prev_breakdown_id
  END as max_jank_neigbour_breakdown_id
FROM internal_event_latency_scroll_breakdowns_next_jank_deltas as next
FULL JOIN internal_event_latency_scroll_breakdowns_prev_jank_deltas as prev
ON next.slice_id = prev.slice_id;

-- Selecting breakdowns which have a maximum ns duration delta as a main causes of a jank for this EventLatency event.
CREATE VIEW internal_event_latency_scroll_jank_cause_top_level
AS
SELECT
  event_latency_id as slice_id,
  track_id,
  event_latency_dur as dur,
  event_latency_ts as ts,
  event_type,
  next_jank,
  prev_jank,
  next_event_latency_id,
  prev_event_latency_id,
  next_delta_dur_ns,
  prev_delta_dur_ns,
  name as cause_of_jank,
  slice_id as max_jank_breakdown_id,
  max_jank_neigbour_breakdown_id,
  MAX(delta_dur_ns) as max_delta_dur_ns
FROM internal_event_latency_scroll_breakdowns_max_jank_deltas
GROUP BY event_latency_id;

-- Selecting sub-breakdowns of the main causes of a jank which have a maximum ns duration delta with a neighbour event's breakdowns.
CREATE VIEW internal_event_latency_scroll_sub_breakdowns_max_deltas
AS
SELECT
  cur_event_latency.slice_id as event_latency_id,
  cur_sub_breakdowns.name as sub_breakdown_name,
  MAX(cur_sub_breakdowns.dur - neighbour_sub_breakdowns.dur) as max_sub_breakdown_delta_dur_ns
FROM internal_event_latency_scroll_jank_cause_top_level as cur_event_latency
LEFT JOIN slices as cur_sub_breakdowns
  ON cur_event_latency.max_jank_breakdown_id = cur_sub_breakdowns.parent_id
LEFT JOIN slices as neighbour_sub_breakdowns
  ON cur_event_latency.max_jank_neigbour_breakdown_id = neighbour_sub_breakdowns.parent_id AND
   cur_sub_breakdowns.name = neighbour_sub_breakdowns.name
GROUP BY cur_event_latency.slice_id, cur_event_latency.max_jank_breakdown_id;

-- Selecting the main cause of jank and its sub-cause of jank.
--
-- @column slice_id               The id of the EventLatency/frame.
-- @column track_id               The track id associated with the EventLatency.
-- @column dur                    The duration of the frame presentation.
-- @column ts                     The timestamp of the frame.
-- @column event_type             The event type.
-- @column next_jank              Whether the subsequent event is janky.
-- @column prev_jank              Whether the previous event is janky.
-- @column next_event_latency_id  The ID of the next frame.
-- @column prev_event_latency_id  The ID of the previous frame.
-- @column next_delta_dur_ns      The duration delta in ns of the next frame.
-- @column prev_delta_dur_ns      The duration delta in ns of the previous
--                                frame.
-- @column cause_of_jank          The stage of EventLatency responsible for the
--                                jank.
-- @column max_jank_breakdown_id  The ID of the stage of maximum jank.
-- @column max_jank_neighbor_breakdown_id     The ID of the stage of maximum
--                                jank in a neighboring frame.
-- @column max_delta_dur_ns       The maximum delta duration of jank in ns.
-- @column sub_cause_of_jank      The sub-stage of the cause_of_jank that is
--                                responsible for the jank, if such a stage
--                                exists.
CREATE VIEW chrome_event_latency_scroll_jank_cause
AS
SELECT
  internal_event_latency_scroll_jank_cause_top_level.*,
  max_sub_breakdowns.sub_breakdown_name as sub_cause_of_jank
FROM internal_event_latency_scroll_jank_cause_top_level
LEFT JOIN internal_event_latency_scroll_sub_breakdowns_max_deltas as max_sub_breakdowns
ON internal_event_latency_scroll_jank_cause_top_level.slice_id = max_sub_breakdowns.event_latency_id;

-- Calculate how often each breakdown is a main cause of a jank for EventLatency events.
--
-- @column cause_of_jank             Cause of jank in the trace
-- @column sub_cause_of_jank         Associated sub-cause of jank in the trace.
-- @column cnt                       # of janks with this cause/subcause pair.
CREATE VIEW chrome_event_latency_scroll_jank_cause_cnt
AS
SELECT
  cause_of_jank,
  sub_cause_of_jank,
  COUNT(*) as cnt
FROM chrome_event_latency_scroll_jank_cause
GROUP BY cause_of_jank, sub_cause_of_jank;
