-- Copyright 2019 The Chromium Authors
-- Use of this source code is governed by a BSD-style license that can be
-- found in the LICENSE file.

-- TODO(b/286187288): Move this dependency to stdlib.
SELECT RUN_METRIC('chrome/event_latency_scroll_jank_cause.sql');
SELECT IMPORT('common.slices');

-- Selects EventLatency slices that correspond with janks in a scroll. This is
-- based on the V2 version of scroll jank metrics.
--
-- @column id INT                     The slice id.
-- @column ts INT                     The start timestamp of the slice.
-- @column dur INT                    The duration of the slice.
-- @column track_id INT               The track_id for the slice.
-- @column name STRING                The name of the slice (EventLatency).
-- @column cause_of_jank STRING       The stage of EventLatency that the caused
--                                    the jank.
-- @column sub_cause_of_jank STRING   The stage of cause_of_jank that caused the
--                                    jank.
CREATE TABLE chrome_janky_event_latencies_v2 AS
  SELECT
    s.id,
    s.ts,
    s.dur,
    s.track_id,
    s.name,
    e.cause_of_jank,
    e.sub_cause_of_jank
FROM slice s
JOIN event_latency_scroll_jank_cause e
  ON s.id = e.slice_id
WHERE
  HAS_DESCENDANT_SLICE_WITH_NAME(
    s.id,
    'SubmitCompositorFrameToPresentationCompositorFrame');

-- Defines slices for all of janky scrolling intervals in a trace.
--
-- @column id            The unique identifier of the janky interval.
-- @column ts            The start timestamp of the janky interval.
-- @column dur           The duration of the janky interval.
CREATE TABLE chrome_scroll_jank_intervals_v2 AS
-- Sub-table to retrieve all janky slice timestamps. Ordering calculations are
-- based on timestamps rather than durations.
WITH janky_latencies AS (
  SELECT
    s.ts AS start_ts,
    s.ts + s.dur AS end_ts
  FROM chrome_janky_event_latencies_v2 s),
-- Determine the local maximum timestamp for janks thus far; this will allow
-- us to coalesce all earlier events up to the maximum.
ordered_jank_end_ts AS (
  SELECT
    *,
    MAX(end_ts) OVER (
      ORDER BY start_ts ROWS BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW)
      AS max_end_ts_so_far
  FROM janky_latencies),
-- Determine the local minimum timestamp for janks thus far; this will allow
-- us to coalesce all later events up to the nearest local maximum.
range_starts AS (
  SELECT
    *,
    CASE
      -- This is a two-pass calculation to calculate the first event in the
      -- group. An event is considered the first event in a group if all events
      -- which started before it also finished the current one started.
      WHEN start_ts <= 1 + LAG(max_end_ts_so_far) OVER (ORDER BY start_ts) THEN 0
      ELSE 1
    END AS range_start
  FROM ordered_jank_end_ts),
-- Assign an id to allow coalescing of individual slices.
range_groups AS (
  SELECT
    *,
    SUM(range_start) OVER (ORDER BY start_ts) AS range_group
  FROM range_starts)
-- Coalesce all slices within an interval.
SELECT
  range_group AS id,
  MIN(start_ts) AS ts,
  MAX(end_ts) - MIN(start_ts) AS dur
FROM range_groups
GROUP BY range_group;
