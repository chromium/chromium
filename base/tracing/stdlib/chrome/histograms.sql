-- Copyright 2023 The Chromium Authors
-- Use of this source code is governed by a BSD-style license that can be
-- found in the LICENSE file.

-- A helper view on top of the histogram events emitted by Chrome.
-- Requires "disabled-by-default-histogram_samples" Chrome category.
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
SELECT
  extract_arg(slice.arg_set_id, "chrome_histogram_sample.name") AS name,
  extract_arg(slice.arg_set_id, "chrome_histogram_sample.sample") AS value,
  ts,
  thread.name AS thread_name,
  thread.utid AS utid,
  thread.tid AS tid,
  process.name AS process_name,
  process.upid AS upid,
  process.pid AS pid
FROM slice
JOIN thread_track
  ON thread_track.id = slice.track_id
JOIN thread
  USING (utid)
JOIN process
  USING (upid)
WHERE
  slice.name = "HistogramSample"
  AND category = "disabled-by-default-histogram_samples";
