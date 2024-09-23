-- Copyright 2024 The Chromium Authors
-- Use of this source code is governed by a BSD-style license that can be
-- found in the LICENSE file.

INCLUDE PERFETTO MODULE slices.with_context;

-- Top level scroll events, with metrics.
CREATE PERFETTO TABLE chrome_scroll_interactions(
  -- Unique id for an individual scroll.
  id INT,
  -- Name of the scroll event.
  name STRING,
  -- Start timestamp of the scroll.
  ts INT,
  -- Duration of the scroll.
  dur INT,
  -- The total number of frames in the scroll.
  frame_count INT,
  -- The total number of vsyncs in the scroll.
  vsync_count INT,
  -- The maximum number of vsyncs missed during any and all janks.
  missed_vsync_max INT,
  -- The total number of vsyncs missed during any and all janks.
  missed_vsync_sum INT,
  -- The number of delayed frames.
  delayed_frame_count INT,
  -- The number of frames that are deemed janky to the human eye after Chrome
  -- has applied its scroll prediction algorithm.
  predictor_janky_frame_count INT,
  -- The process id this event occurred on.
  renderer_upid INT
) AS
WITH scroll_metrics AS (
  SELECT
    id,
    ts,
    dur,
    EXTRACT_ARG(arg_set_id, 'scroll_metrics.frame_count')
      AS frame_count,
    EXTRACT_ARG(arg_set_id, 'scroll_metrics.vsync_count')
      AS vsync_count,
    EXTRACT_ARG(arg_set_id, 'scroll_metrics.missed_vsync_max')
      AS missed_vsync_max,
    EXTRACT_ARG(arg_set_id, 'scroll_metrics.missed_vsync_sum')
      AS missed_vsync_sum,
    EXTRACT_ARG(arg_set_id, 'scroll_metrics.delayed_frame_count')
      AS delayed_frame_count,
    EXTRACT_ARG(arg_set_id, 'scroll_metrics.predictor_janky_frame_count')
      AS predictor_janky_frame_count,
    upid AS renderer_upid
  FROM process_slice
  WHERE name = 'Scroll'
)
SELECT
  id,
  'Scroll' AS name,
  ts,
  dur,
  frame_count,
  vsync_count,
  missed_vsync_max,
  missed_vsync_sum,
  delayed_frame_count,
  predictor_janky_frame_count,
  renderer_upid
FROM scroll_metrics;