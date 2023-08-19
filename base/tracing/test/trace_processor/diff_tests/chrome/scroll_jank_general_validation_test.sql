-- Copyright 2023 The Chromium Authors
-- Use of this source code is governed by a BSD-style license that can be
-- found in the LICENSE file.

SELECT RUN_METRIC('chrome/scroll_jank.sql');

SELECT (
  -- There are only two valid scrolls (one additional scroll is missing a begin
  -- and the other is missing an end).
  SELECT COUNT(*) FROM joined_scroll_begin_and_end
) AS total,
(
  -- Of the two valid scrolls
  -- gesture_scroll_id: 2708
  --     starts at: 544958000403
  --     ends at:   545859896829
  --     adds dur:     901896426 nanoseconds of scrolling.
  --
  -- gesture_scroll_id: 2917
  --     starts at: 546027000403
  --     ends at:   546753574829
  --     adds dur:     726574426 nanoseconds of scrolling.
  -- This means we should have scroll_dur = 1628470852
  SELECT SUM(scroll_dur) FROM (
    SELECT
      gesture_scroll_id, max(maybe_gesture_end) - begin_ts AS scroll_dur
    FROM scroll_jank
    GROUP BY gesture_scroll_id
  )
) AS scroll_dur,
(
  -- This can be verified by the following simple query to ensure the end result
  -- in scroll_jank table is sane. The result should be 139.
  -- SELECT
  --   COUNT(*)
  -- FROM slice
  -- WHERE
  --    name = "InputLatency::GestureScrollUpdate" AND
  --    NOT EXTRACT_ARG(arg_set_id, 'chrome_latency_info.is_coalesced') AND
  --    (
  --    EXTRACT_ARG(arg_set_id, 'chrome_latency_info.gesture_scroll_id') = 2708
  --    OR
  --    EXTRACT_ARG(arg_set_id, 'chrome_latency_info.gesture_scroll_id') = 2917
  --    )
  SELECT COUNT(*) FROM scroll_jank
) AS non_coalesced_updates,
(
  -- This can be verified by the following simple query as above but replace
  -- COUNT(*) with SUM(dur). The result should be 3974685214.
  SELECT SUM(dur) FROM scroll_jank
) AS non_coalesced_dur,
(
  -- This was found by running the previous metric before porting on the
  -- example trace.
  SELECT COUNT(*) FROM scroll_jank WHERE jank
) AS non_coalesced_janky_updates,
(
  -- This was found by running the previous metric before porting on the
  -- example trace, and also manually summing them.
  SELECT SUM(dur) FROM scroll_jank WHERE jank
) AS non_coalesced_janky_dur,
(
  -- This is floor((non_coalesced_janky_dur/non_coalesced_dur) * 100) in SQLite.
  SELECT
    CAST((CAST((SELECT SUM(dur) FROM scroll_jank WHERE jank) AS FLOAT)
      / CAST((SELECT SUM(dur) FROM scroll_jank) AS FLOAT)) * 100 AS INT)
) AS janky_percentage,
(
  SELECT avg_vsync_interval FROM joined_scroll_begin_and_end
) AS avg_vsync_interval;
