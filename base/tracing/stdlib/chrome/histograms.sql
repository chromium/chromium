-- Copyright 2023 The Chromium Authors
-- Use of this source code is governed by a BSD-style license that can be
-- found in the LICENSE file.

-- A helper view on top of the histogram events emitted by Chrome.
-- Requires "disabled-by-default-histogram_samples" Chrome category.
CREATE PERFETTO TABLE chrome_histograms(
  -- The name of the histogram.
  name STRING,
  -- The value of the histogram sample.
  value INT,
  -- Alias of |slice.ts|.
  ts INT,
  -- Thread name.
  thread_name STRING,
  -- Utid of the thread.
  utid INT,
  -- Tid of the thread.
  tid INT,
  -- Process name.
  process_name STRING,
  -- Upid of the process.
  upid INT,
  -- Pid of the process.
  pid INT
) AS
SELECT
  extract_arg(slice.arg_set_id, "chrome_histogram_sample.name") as name,
  extract_arg(slice.arg_set_id, "chrome_histogram_sample.sample") as value,
  ts,
  thread.name as thread_name,
  thread.utid as utid,
  thread.tid as tid,
  process.name as process_name,
  process.upid as upid,
  process.pid as pid
FROM slice
JOIN thread_track ON thread_track.id = slice.track_id
JOIN thread USING (utid)
JOIN process USING (upid)
WHERE
  slice.name = "HistogramSample"
  AND category = "disabled-by-default-histogram_samples";