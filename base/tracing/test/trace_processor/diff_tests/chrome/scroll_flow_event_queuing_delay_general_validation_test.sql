-- Copyright 2023 The Chromium Authors
-- Use of this source code is governed by a BSD-style license that can be
-- found in the LICENSE file.
SELECT RUN_METRIC('chrome/scroll_flow_event_queuing_delay.sql');

SELECT
  -- Each trace_id (in our example trace not true in general) has 8 steps. There
  -- are 139 scrolls. So we expect 1112 rows in total 72 of which are janky.
  (
    SELECT
      COUNT(*)
    FROM (
      SELECT
        trace_id,
        COUNT(*)
      FROM scroll_flow_event_queuing_delay
      GROUP BY trace_id
    )
  ) AS total_scroll_updates,
  (
    SELECT COUNT(*) FROM scroll_flow_event_queuing_delay
  ) AS total_flow_event_steps,
  (
    SELECT COUNT(*) FROM scroll_flow_event_queuing_delay WHERE jank
  ) AS total_janky_flow_event_steps,
  (
    SELECT COUNT(*) FROM (SELECT step FROM scroll_flow_event_queuing_delay GROUP BY step)
  ) AS number_of_unique_steps;
