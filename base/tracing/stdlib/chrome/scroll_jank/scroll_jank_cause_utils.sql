-- Copyright 2023 The Chromium Authors
-- Use of this source code is governed by a BSD-style license that can be
-- found in the LICENSE file.


-- Retrieve the thread id of the thread on a particular process, if the name of
-- that process is known. Returns an error if there are multiple threads in
-- the given process with the same name.
CREATE PERFETTO FUNCTION internal_find_utid_by_upid_and_name(
  -- Unique process id
  upid INT,
  -- The name of the thread
  thread_name STRING)
RETURNS TABLE (
  -- Unique thread id.
  utid INT
) AS
SELECT
  DISTINCT utid
FROM thread
WHERE upid = $upid
  AND name = $thread_name;

-- Function to retrieve the track id of the thread on a particular process if
-- there are any slices during a particular EventLatency slice duration; this
-- upid/thread combination refers to a cause of Scroll Jank.
CREATE PERFETTO FUNCTION chrome_select_scroll_jank_cause_track(
  -- The slice id of an EventLatency slice.
  event_latency_id INT,
  -- The process id that the thread is on.
  upid INT,
  -- The name of the thread.
  thread_name STRING)
RETURNS TABLE (
  -- The track id associated with |thread| on the process with |upid|.
  track_id INT
) AS
SELECT
 DISTINCT track_id
FROM thread_slice
WHERE utid IN
  (
    SELECT
      utid
    FROM internal_find_utid_by_upid_and_name($upid, $thread_name)
  )
  AND ts >= (SELECT ts FROM slice WHERE id = $event_latency_id LIMIT 1)
  AND ts <= (SELECT ts + dur FROM slice WHERE id = $event_latency_id LIMIT 1);
