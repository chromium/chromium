-- Copyright 2023 The Chromium Authors
-- Use of this source code is governed by a BSD-style license that can be
-- found in the LICENSE file.

INCLUDE PERFETTO MODULE slices.with_context;

-- Function to retrieve the upid for a surfaceflinger, as these are attributed
-- to the GPU but are recorded on a different data source (and track group).
CREATE PERFETTO FUNCTION _get_process_id_for_surfaceflinger()
-- The process id for surfaceflinger.
RETURNS INT AS
SELECT
 upid
FROM process
WHERE name GLOB '*surfaceflinger*'
LIMIT 1;

-- Map a generic process type to a specific name or substring of a name that
-- can be found in the trace process table.
CREATE PERFETTO TABLE _process_type_to_name (
  -- The process type: one of 'Browser' or 'GPU'.
  process_type STRING,
  -- The process name for Chrome traces.
  process_name STRING,
  -- Substring identifying the process for system traces.
  process_glob STRING
) AS
WITH process_names (
  process_type,
  process_name,
  process_glob
  )
AS (
VALUES
  ('Browser', 'Browser', '*.chrome'),
  ('GPU', 'Gpu', '*.chrome*:privileged_process*'))
SELECT
  process_type,
  process_name,
  process_glob
FROM process_names;

CREATE PERFETTO FUNCTION _get_process_name(
  -- The process type: one of 'Browser' or 'GPU'.
  type STRING
)
-- The process name
RETURNS STRING AS
SELECT
    process_name
FROM _process_type_to_name
WHERE process_type = $type
LIMIT 1;

CREATE PERFETTO FUNCTION _get_process_glob(
  -- The process type: one of 'Browser' or 'GPU'.
  type STRING
)
-- A substring of the process name that can be used in GLOB calculations.
RETURNS STRING AS
SELECT
    process_glob
FROM _process_type_to_name
WHERE process_type = $type
LIMIT 1;

-- TODO(b/309937901): Add chrome instance id for multiple chromes/webviews in a
-- trace, as this may result in  multiple browser and GPU processes.
-- Function to retrieve the chrome process ID for a specific process type. Does
-- not retrieve the Renderer process, as this is determined when the
-- EventLatency is known. See function
-- _get_renderer_upid_for_event_latency below.
CREATE PERFETTO FUNCTION _get_process_id_by_type(
  -- The process type: one of 'Browser' or 'GPU'.
  type STRING
)
RETURNS TABLE (
    -- The process id for the process type.
    upid INT
) AS
SELECT
  upid
FROM process
WHERE name = _get_process_name($type)
  OR name GLOB _get_process_glob($type);

-- Function to retrieve the chrome process ID that a given EventLatency slice
-- occurred on. This is the Renderer process.
CREATE PERFETTO FUNCTION _get_renderer_upid_for_event_latency(
  -- The slice id for an EventLatency slice.
  id INT
)
-- The process id for an EventLatency slice. This is the Renderer process.
RETURNS INT AS
SELECT
  upid
FROM process_slice
WHERE id = $id;

-- Helper function to retrieve all of the upids for a given process, thread,
-- or EventLatency.
CREATE PERFETTO FUNCTION _processes_by_type_for_event_latency(
  -- The process type that the thread is on: one of 'Browser', 'Renderer' or
  -- 'GPU'.
  type STRING,
  -- The name of the thread.
  thread STRING,
  -- The slice id of an EventLatency slice.
  event_latency_id INT)
RETURNS TABLE (
    upid INT
) AS
WITH all_upids AS (
  -- Renderer process upids
  SELECT
    $type AS process,
    $thread AS thread,
    $event_latency_id AS event_latency_id,
    _get_renderer_upid_for_event_latency($event_latency_id) AS upid
  WHERE $type = 'Renderer'
  UNION ALL
  -- surfaceflinger upids
  SELECT
    $type AS process,
    $thread AS thread,
    $event_latency_id AS event_latency_id,
    _get_process_id_for_surfaceflinger() AS upid
  WHERE $type = 'GPU' AND $thread = 'surfaceflinger'
  UNION ALL
  -- Generic Browser and GPU process upids
  SELECT
    $type AS process,
    $thread AS thread,
    $event_latency_id AS event_latency_id,
    upid
  FROM _get_process_id_by_type($type)
  WHERE $type = 'Browser'
    OR ($type = 'GPU' AND $thread != 'surfaceflinger')
)
SELECT
  upid
FROM all_upids;

-- Function to retrieve the thread id of the thread on a particular process if
-- there are any slices during a particular EventLatency slice duration; this
-- upid/thread combination refers to a cause of Scroll Jank.
CREATE PERFETTO FUNCTION chrome_select_scroll_jank_cause_thread(
  -- The slice id of an EventLatency slice.
  event_latency_id INT,
  -- The process type that the thread is on: one of 'Browser', 'Renderer' or
  -- 'GPU'.
  process_type STRING,
  -- The name of the thread.
  thread_name STRING)
RETURNS TABLE (
  -- The utid associated with |thread| on the process with |upid|.
  utid INT
) AS
WITH threads AS (
  SELECT
    utid
  FROM thread
  WHERE upid IN
    (
      SELECT DISTINCT
        upid
      FROM _processes_by_type_for_event_latency(
        $process_type,
        $thread_name,
        $event_latency_id)
    )
    AND name = $thread_name
)
SELECT
 DISTINCT utid
FROM thread_slice
WHERE utid IN
  (
    SELECT
      utid
    FROM threads
  )
  AND ts >= (SELECT ts FROM slice WHERE id = $event_latency_id LIMIT 1)
  AND ts <= (SELECT ts + dur FROM slice WHERE id = $event_latency_id LIMIT 1);
