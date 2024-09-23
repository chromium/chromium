-- Copyright 2024 The Chromium Authors
-- Use of this source code is governed by a BSD-style license that can be
-- found in the LICENSE file.

-- This file implements the scrolling predictor jank metric, as is
-- implemented in cc/metrics/predictor_jank_tracker.cc. See comments in that
-- file to get additional context on how the metric is implemented.
--
-- "Delta" here refers to how much (in pixels) the page offset changed for a
-- given frame due to a scroll.
--
-- For more details please check the following document:
-- http://doc/1Y0u0Tq5eUZff75nYUzQVw6JxmbZAW9m64pJidmnGWsY.

INCLUDE PERFETTO MODULE chrome.scroll_jank.scroll_offsets;

-- The maximum delta value between three consecutive frames, used to determine
-- whether the sequence in the scroll is fast or slow; the sequence speed is
-- used to determine whether the sequence includes any predictor jank.
CREATE PERFETTO FUNCTION _get_slow_scroll_delta_threshold()
RETURNS DOUBLE AS
SELECT 7.0;

-- The threshold for the ratio of the delta of middle frame to tbe deltas of its
-- neighbors in a sequence of three frames, if the sequence is considered
-- "slow".
CREATE PERFETTO FUNCTION _get_slow_scroll_janky_threshold()
RETURNS DOUBLE AS
SELECT 1.4;

-- The threshold for the ratio of the delta of middle frame to tbe deltas of its
-- neighbors in a sequence of three frames, if the sequence is considered
-- "fast".
CREATE PERFETTO FUNCTION _get_fast_scroll_janky_threshold()
RETURNS DOUBLE AS
SELECT 1.2;

-- Determine the acceptable threshold (see _get_slow_scroll_janky_threshold()
-- and _get_fast_scroll_janky_threshold()) based on the maximum delta value
-- between three consecutive frames.
CREATE PERFETTO FUNCTION _get_scroll_jank_threshold(
  d1 DOUBLE,
  d2 DOUBLE,
  d3 DOUBLE
)
RETURNS DOUBLE AS
SELECT
  CASE
    WHEN MAX(MAX($d1, $d2), $d3) <= _get_slow_scroll_delta_threshold()
      THEN _get_slow_scroll_janky_threshold()
    ELSE _get_fast_scroll_janky_threshold()
  END;

-- Calculate the predictor jank of three consecutive frames, if it is above the
-- threshold. Anything below the threshold is not considered jank.
CREATE PERFETTO FUNCTION _get_predictor_jank(
  d1 DOUBLE,
  d2 DOUBLE,
  d3 DOUBLE,
  threshold DOUBLE
)
RETURNS DOUBLE AS
SELECT
  CASE
    WHEN $d2/MAX($d1, $d3) >= $threshold
      THEN $d2/MAX($d1, $d3) - $threshold
    WHEN MIN($d1, $d3)/$d2 >= $threshold
      THEN MIN($d1, $d3)/$d2 - $threshold
    ELSE 0
  END;

CREATE PERFETTO TABLE _deltas_and_neighbors AS
SELECT
  scroll_id,
  event_latency_slice_id,
  scroll_update_id,
  ts,
  delta_y,
  relative_offset_y,
  LAG(IFNULL(delta_y, 0.0))
    OVER (PARTITION BY scroll_id ORDER BY ts ASC) AS prev_delta,
  LEAD(IFNULL(delta_y, 0.0))
    OVER (PARTITION BY scroll_id ORDER BY ts ASC) AS next_delta
FROM chrome_presented_scroll_offsets;

CREATE PERFETTO TABLE _deltas_and_neighbors_with_threshold AS
SELECT
  scroll_id,
  event_latency_slice_id,
  scroll_update_id,
  ts,
  delta_y,
  relative_offset_y,
  prev_delta,
  next_delta,
  _get_scroll_jank_threshold(ABS(prev_delta), ABS(delta_y), ABS(next_delta))
    AS delta_threshold
FROM _deltas_and_neighbors
WHERE delta_y IS NOT NULL
  AND prev_delta IS NOT NULL
  AND next_delta IS NOT NULL;

-- The scrolling offsets and predictor jank values for the actual (applied)
-- scroll events.
CREATE PERFETTO TABLE chrome_predictor_error(
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
  present_ts INT,
  -- The delta in raw coordinates between this presented EventLatency and the
  -- previous presented frame.
  delta_y DOUBLE,
  -- The pixel offset of this presented EventLatency compared to the initial
  -- one.
  relative_offset_y DOUBLE,
  -- The delta in raw coordinates of the previous scroll update event.
  prev_delta DOUBLE,
  -- The delta in raw coordinates of the subsequent scroll update event.
  next_delta DOUBLE,
  -- The jank value based on the discrepancy between scroll predictor
  -- coordinates and the actual deltas between scroll update events.
  predictor_jank DOUBLE,
  -- The threshold used to determine if jank occurred.
  delta_threshold DOUBLE
)
AS
SELECT
  scroll_id,
  event_latency_slice_id,
  scroll_update_id,
  ts AS present_ts,
  delta_y,
  relative_offset_y,
  prev_delta,
  next_delta,
  _get_predictor_jank(
    ABS(prev_delta), ABS(delta_y), ABS(next_delta), delta_threshold)
      AS predictor_jank,
  delta_threshold
FROM _deltas_and_neighbors_with_threshold;
