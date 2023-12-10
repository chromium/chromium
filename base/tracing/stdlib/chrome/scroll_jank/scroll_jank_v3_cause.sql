-- Copyright 2023 The Chromium Authors
-- Use of this source code is governed by a BSD-style license that can be
-- found in the LICENSE file.

-- Helper functions for scroll_jank_v3 metric computation.

INCLUDE PERFETTO MODULE common.slices;


-- Given two slice Ids A and B, find the maximum difference
-- between the durations of it's direct children with matching names
-- for example if slice A has children named (X, Y, Z) with durations of (10, 10, 5)
-- and slice B has children named (X, Y) with durations of (9, 9), the function will return
-- the slice id of the slice named Z that is A's child, as no matching slice named Z was found
-- under B, making 5 - 0 = 5 the maximum delta between both slice's direct children
CREATE PERFETTO FUNCTION chrome_get_v3_jank_cause_id(
  -- The slice id of the parent slice that we want to cause among it's children.
  janky_slice_id LONG,
  -- The slice id of the parent slice that's the reference in comparison to
  -- |janky_slice_id|.
  prev_slice_id LONG
)
-- The slice id of the breakdown that has the maximum duration delta.
RETURNS LONG AS
WITH
  current_breakdowns AS (
    SELECT
      *
    FROM direct_children_slice($janky_slice_id)
  ),
  prev_breakdowns AS (
    SELECT
      *
    FROM direct_children_slice($prev_slice_id)
  ),
  joint_breakdowns AS (
    SELECT
      cur.id AS breakdown_id,
      (cur.dur - COALESCE(prev.dur, 0)) AS breakdown_delta
    FROM current_breakdowns cur
    LEFT JOIN prev_breakdowns prev ON
      cur.name = prev.name
  ),
  max_breakdown AS (
    SELECT
      MAX(breakdown_delta) AS breakdown_delta,
      breakdown_id
    FROM joint_breakdowns
  )
  SELECT
    breakdown_id
  FROM max_breakdown;
