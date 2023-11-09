-- Copyright 2023 The Chromium Authors
-- Use of this source code is governed by a BSD-style license that can be
-- found in the LICENSE file.

-- A simple table that checks the time between VSync (this can be used to
-- determine if we're refreshing at 90 FPS or 60 FPS).
--
-- Note: In traces without the "Java" category there will be no VSync
--       TraceEvents and this table will be empty.
CREATE PERFETTO TABLE chrome_vsync_intervals(
  -- Slice id of the vsync slice.
  slice_id INT,
  -- Timestamp of the vsync slice.
  ts INT,
  -- Duration of the vsync slice.
  dur INT,
  -- Track id of the vsync slice.
  track_id INT,
  -- Duration until next vsync arrives.
  time_to_next_vsync INT
) AS
SELECT
  slice_id,
  ts,
  dur,
  track_id,
  LEAD(ts) OVER(PARTITION BY track_id ORDER BY ts) - ts AS time_to_next_vsync
FROM slice
WHERE name = "VSync"
ORDER BY track_id, ts;

-- Function: compute the average Vysnc interval of the
-- gesture (hopefully this would be either 60 FPS for the whole gesture or 90
-- FPS but that isnt always the case) on the given time segment.
-- If the trace doesnt contain the VSync TraceEvent we just fall back on
-- assuming its 60 FPS (this is the 1.6e+7 in the COALESCE which
-- corresponds to 16 ms or 60 FPS).
CREATE PERFETTO FUNCTION chrome_calculate_avg_vsync_interval(
  -- Interval start time.
  begin_ts LONG,
  -- Interval end time.
  end_ts LONG
)
-- The average vsync interval on this time segment
-- or 1.6e+7, if trace doesn't contain the VSync TraceEvent.
RETURNS FLOAT AS
SELECT
  COALESCE((
    SELECT
      CAST(AVG(time_to_next_vsync) AS FLOAT)
    FROM chrome_vsync_intervals in_query
    WHERE
      time_to_next_vsync IS NOT NULL AND
      in_query.ts > $begin_ts AND
      in_query.ts < $end_ts
  ), 1e+9 / 60);
