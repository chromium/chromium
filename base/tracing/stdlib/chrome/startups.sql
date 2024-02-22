-- Copyright 2023 The Chromium Authors
-- Use of this source code is governed by a BSD-style license that can be
-- found in the LICENSE file.

INCLUDE PERFETTO MODULE slices.with_context;

-- Access all startups, including those that don't lead to any visible content.
-- If TimeToFirstVisibleContent is available, then this event will be the
-- main event of the startup. Otherwise, the event for the start timestamp will
-- be used.
CREATE PERFETTO VIEW _startup_start_events AS
WITH
starts AS (
  SELECT
    name,
    EXTRACT_ARG(arg_set_id, 'startup.activity_id') AS activity_id,
    ts,
    dur,
    upid AS browser_upid
  FROM thread_slice
  WHERE name = 'Startup.ActivityStart'
),
times_to_first_visible_content AS (
  SELECT
    name,
    EXTRACT_ARG(arg_set_id, 'startup.activity_id') AS activity_id,
    ts,
    dur,
    upid AS browser_upid
  FROM process_slice
  WHERE name = 'Startup.TimeToFirstVisibleContent2'
),
all_activity_ids AS (
  SELECT
    DISTINCT activity_id,
    browser_upid
  FROM starts
  UNION ALL
  SELECT
    DISTINCT activity_id,
    browser_upid
  FROM times_to_first_visible_content
),
activity_ids AS (
  SELECT
    DISTINCT activity_id,
    browser_upid
  FROM all_activity_ids
)
SELECT
  activity_ids.activity_id,
  'Startup' AS name,
  IFNULL(times_to_first_visible_content.ts, starts.ts) AS startup_begin_ts,
  times_to_first_visible_content.ts +
    times_to_first_visible_content.dur AS first_visible_content_ts,
  activity_ids.browser_upid
FROM activity_ids
  LEFT JOIN times_to_first_visible_content using(activity_id, browser_upid)
  LEFT JOIN starts using(activity_id, browser_upid);

-- Chrome launch causes, not recorded at start time; use the activity id to
-- join with the actual startup events.
CREATE PERFETTO VIEW _launch_causes AS
SELECT
  EXTRACT_ARG(arg_set_id, 'startup.activity_id') AS activity_id,
  EXTRACT_ARG(arg_set_id, 'startup.launch_cause') AS launch_cause,
  upid AS browser_upid
FROM thread_slice
WHERE name = 'Startup.LaunchCause';

-- Chrome startups, including launch cause.
CREATE PERFETTO TABLE chrome_startups(
  -- Unique ID
  id INT,
  -- Chrome Activity event id of the launch.
  activity_id INT,
  -- Name of the launch start event.
  name STRING,
  -- Timestamp that the startup occurred.
  startup_begin_ts INT,
  -- Timestamp to the first visible content.
  first_visible_content_ts INT,
  -- Launch cause. See Startup.LaunchCauseType in chrome_track_event.proto.
  launch_cause STRING,
  -- Process ID of the Browser where the startup occurred.
  browser_upid INT
) AS
SELECT
  ROW_NUMBER() OVER (ORDER BY start_events.startup_begin_ts) AS id,
  start_events.activity_id,
  start_events.name,
  start_events.startup_begin_ts,
  start_events.first_visible_content_ts,
  launches.launch_cause,
  start_events.browser_upid
FROM _startup_start_events start_events
  LEFT JOIN _launch_causes launches
  USING(activity_id, browser_upid);
