-- Copyright 2023 The Chromium Authors
-- Use of this source code is governed by a BSD-style license that can be
-- found in the LICENSE file.

-- Defines slices for all of the individual scrolls in a trace based on the
-- LatencyInfo-based scroll definition.
--
-- @column id            The unique identifier of the scroll.
-- @column ts            The start timestamp of the scroll.
-- @column dur           The duration of the scroll.
--
-- NOTE: this view of top level scrolls is based on the LatencyInfo definition
-- of a scroll, which differs subtly from the definition based on
-- EventLatencies.
-- TODO(b/278684408): add support for tracking scrolls across multiple Chrome/
-- WebView instances. Currently gesture_scroll_id unique within an instance, but
-- is not unique across multiple instances. Switching to an EventLatency based
-- definition of scrolls should resolve this.
CREATE PERFETTO TABLE chrome_scrolls AS
WITH all_scrolls AS (
  SELECT
    name,
    ts,
    dur,
    extract_arg(arg_set_id, 'chrome_latency_info.gesture_scroll_id') AS scroll_id
  FROM slice
  WHERE name GLOB 'InputLatency::GestureScroll*'
  AND extract_arg(arg_set_id, 'chrome_latency_info.gesture_scroll_id') IS NOT NULL
),
scroll_starts AS (
  SELECT
    scroll_id,
    MIN(ts) AS scroll_start_ts
  FROM all_scrolls
  WHERE name = 'InputLatency::GestureScrollBegin'
  GROUP BY scroll_id
), scroll_ends AS (
  SELECT
    scroll_id,
    MIN(ts) AS scroll_end_ts
  FROM all_scrolls
  WHERE name = 'InputLatency::GestureScrollEnd'
  GROUP BY scroll_id
)
SELECT
  sa.scroll_id AS id,
  MIN(ts) AS ts,
  CAST(MAX(ts + dur) - MIN(ts) AS INT) AS dur,
  ss.scroll_start_ts AS scroll_start_ts,
  se.scroll_end_ts AS scroll_end_ts
FROM all_scrolls sa
  LEFT JOIN scroll_starts ss ON
    sa.scroll_id = ss.scroll_id
  LEFT JOIN scroll_ends se ON
    sa.scroll_id = se.scroll_id
GROUP BY sa.scroll_id;

-- Defines slices for all of scrolls intervals in a trace based on the scroll
-- definition in chrome_scrolls. Note that scrolls may overlap (particularly in
-- cases of jank/broken traces, etc); so scrolling intervals are not exactly the
-- same as individual scrolls.
--
-- @column id            The unique identifier of the scroll interval. This may
--                       span multiple scrolls if they overlap.
-- @column ts            The start timestamp of the scroll interval.
-- @column dur           The duration of the scroll interval.
CREATE VIEW chrome_scrolling_intervals AS
WITH all_scrolls AS (
  SELECT
    id AS scroll_id,
    s.ts AS start_ts,
    s.ts + s.dur AS end_ts
  FROM chrome_scrolls s),
ordered_end_ts AS (
  SELECT
    *,
    MAX(end_ts) OVER (ORDER BY start_ts) AS max_end_ts_so_far
  FROM all_scrolls),
range_starts AS (
  SELECT
    *,
    CASE
      WHEN start_ts <= 1 + LAG(max_end_ts_so_far) OVER (ORDER BY start_ts) THEN 0
      ELSE 1
    END AS range_start
  FROM ordered_end_ts),
range_groups AS (
  SELECT
    *,
    SUM(range_start) OVER (ORDER BY start_ts) AS range_group
  FROM range_starts)
SELECT
  range_group AS id,
  GROUP_CONCAT(scroll_id) AS scroll_ids,
  MIN(start_ts) AS ts,
  MAX(end_ts) - MIN(start_ts) AS dur
FROM range_groups
GROUP BY range_group;
