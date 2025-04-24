-- Copyright 2023 The Chromium Authors
-- Use of this source code is governed by a BSD-style license that can be
-- found in the LICENSE file.

-- A helper view on top of the histogram events emitted by Chrome.
-- Requires "disabled-by-default-histogram_samples" Chrome category or the
-- "org.chromium.histogram_sample" data source.
CREATE PERFETTO TABLE chrome_histograms (
  -- The name of the histogram.
  name STRING,
  -- The value of the histogram sample.
  value LONG,
  -- Alias of |slice.ts|.
  ts TIMESTAMP,
  -- Thread name.
  thread_name STRING,
  -- Utid of the thread.
  utid LONG,
  -- Tid of the thread.
  tid LONG,
  -- Process name.
  process_name STRING,
  -- Upid of the process.
  upid LONG,
  -- Pid of the process.
  pid LONG
) AS
WITH
  -- Select raw histogram sample slices from the slice table.
  hist AS (
    SELECT
      extract_arg(slice.arg_set_id, 'chrome_histogram_sample.name') AS name,
      extract_arg(slice.arg_set_id, 'chrome_histogram_sample.sample') AS value,
      ts,
      track_id
    FROM slice
    WHERE
      slice.name = "HistogramSample"
      AND category = "disabled-by-default-histogram_samples"
  )
-- Part 1: join histogram samples emitted via the track event category.
-- These samples are associated with a specific thread track.
SELECT
  hist.name,
  hist.value,
  hist.ts,
  thread.name AS thread_name,
  thread.utid AS utid,
  thread.tid AS tid,
  process.name AS process_name,
  process.upid,
  process.pid
FROM hist
JOIN thread_track
  ON thread_track.id = hist.track_id
JOIN thread
  USING (utid)
JOIN process
  USING (upid)
UNION ALL
-- Part 2: Join histogram samples emitted via the
-- "org.chromium.histogram_sample" data source. These samples are associated
-- with a process track.
SELECT
  hist.name,
  hist.value,
  hist.ts,
  NULL AS thread_name,
  NULL AS utid,
  NULL AS tid,
  process.name AS process_name,
  process.upid,
  process.pid
FROM hist
JOIN process_track
  ON process_track.id = hist.track_id
JOIN process
  USING (upid);
