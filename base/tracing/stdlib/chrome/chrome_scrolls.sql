-- Copyright 2023 The Chromium Authors
-- Use of this source code is governed by a BSD-style license that can be
-- found in the LICENSE file.

INCLUDE PERFETTO MODULE chrome.scroll_jank.utils;

-- Defines slices for all of the individual scrolls in a trace based on the
-- LatencyInfo-based scroll definition.
--
-- NOTE: this view of top level scrolls is based on the LatencyInfo definition
-- of a scroll, which differs subtly from the definition based on
-- EventLatencies.
-- TODO(b/278684408): add support for tracking scrolls across multiple Chrome/
-- WebView instances. Currently gesture_scroll_id unique within an instance, but
-- is not unique across multiple instances. Switching to an EventLatency based
-- definition of scrolls should resolve this.
CREATE PERFETTO TABLE chrome_scrolls(
  -- The unique identifier of the scroll.
  id INT,
  -- The start timestamp of the scroll.
  ts INT,
  -- The duration of the scroll.
  dur INT,
  -- The earliest timestamp of the EventLatency slice of the GESTURE_SCROLL_BEGIN type for the
  -- corresponding scroll id.
  gesture_scroll_begin_ts INT,
  -- The earliest timestamp of the EventLatency slice of the GESTURE_SCROLL_END type /
  -- the latest timestamp of the EventLatency slice of the GESTURE_SCROLL_UPDATE type for the
  -- corresponding scroll id.
  gesture_scroll_end_ts INT
) AS
WITH all_scrolls AS (
  SELECT
    event_type AS name,
    ts,
    dur,
    scroll_id
  FROM chrome_gesture_scroll_events
),
scroll_starts AS (
  SELECT
    scroll_id,
    MIN(ts) AS gesture_scroll_begin_ts
  FROM all_scrolls
  WHERE name = 'GESTURE_SCROLL_BEGIN'
  GROUP BY scroll_id
),
scroll_ends AS (
  SELECT
    scroll_id,
    MAX(ts) AS gesture_scroll_end_ts
  FROM all_scrolls
  WHERE name IN (
    'GESTURE_SCROLL_UPDATE',
    'FIRST_GESTURE_SCROLL_UPDATE',
    'INERTIAL_GESTURE_SCROLL_UPDATE',
    'GESTURE_SCROLL_END'
  )
  GROUP BY scroll_id
)
SELECT
  sa.scroll_id AS id,
  MIN(ts) AS ts,
  CAST(MAX(ts + dur) - MIN(ts) AS INT) AS dur,
  ss.gesture_scroll_begin_ts AS gesture_scroll_begin_ts,
  se.gesture_scroll_end_ts AS gesture_scroll_end_ts
FROM all_scrolls sa
  LEFT JOIN scroll_starts ss ON
    sa.scroll_id = ss.scroll_id
  LEFT JOIN scroll_ends se ON
    sa.scroll_id = se.scroll_id
GROUP BY sa.scroll_id;
