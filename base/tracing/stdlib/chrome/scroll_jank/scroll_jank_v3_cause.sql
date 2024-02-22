-- Copyright 2023 The Chromium Authors
-- Use of this source code is governed by a BSD-style license that can be
-- found in the LICENSE file.

-- Finds all slices with a direct parent with the given parent_id.
CREATE PERFETTO FUNCTION _direct_children_slice(
  -- Id of the parent slice.
  parent_id LONG)
RETURNS TABLE(
  -- Alias for `slice.id`.
  id LONG,
  -- Alias for `slice.type`.
  type STRING,
  -- Alias for `slice.ts`.
  ts LONG,
  -- Alias for `slice.dur`.
  dur LONG,
  -- Alias for `slice.category`.
  category LONG,
  -- Alias for `slice.name`.
  name STRING,
  -- Alias for `slice.track_id`.
  track_id LONG,
  -- Alias for `slice.depth`.
  depth LONG,
  -- Alias for `slice.parent_id`.
  parent_id LONG,
  -- Alias for `slice.arg_set_id`.
  arg_set_id LONG,
  -- Alias for `slice.thread_ts`.
  thread_ts LONG,
  -- Alias for `slice.thread_dur`.
  thread_dur LONG
) AS
SELECT
  slice.id,
  slice.type,
  slice.ts,
  slice.dur,
  slice.category,
  slice.name,
  slice.track_id,
  slice.depth,
  slice.parent_id,
  slice.arg_set_id,
  slice.thread_ts,
  slice.thread_dur
FROM slice
WHERE parent_id = $parent_id;

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
    FROM _direct_children_slice($janky_slice_id)
  ),
  prev_breakdowns AS (
    SELECT
      *
    FROM _direct_children_slice($prev_slice_id)
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
