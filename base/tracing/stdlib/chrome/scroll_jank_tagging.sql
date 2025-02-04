-- Copyright 2025 The Chromium Authors
-- Use of this source code is governed by a BSD-style license that can be
-- found in the LICENSE file.

INCLUDE PERFETTO MODULE chrome.chrome_scrolls;
INCLUDE PERFETTO MODULE time.conversion;

-- A helper table to avoid manually filtering `chrome_scroll_frame_info`
-- multiple times, as well as containing a few additional statistics.
CREATE PERFETTO TABLE _chrome_janky_scroll_frames AS
SELECT
  *,
  (
    cast_double!(previous_last_input_to_first_input_generation_dur)
    / vsync_interval_dur
  ) AS previous_input_delta_to_vsync_ratio,
  (
    cast_double!(first_input_compositor_dispatch_to_on_begin_frame_delay_dur)
    / vsync_interval_dur
  ) AS wait_for_begin_frame_delta_to_vsync_ratio,
  (
    cast_double!(viz_wait_for_draw_dur)
    / vsync_interval_dur
  ) AS viz_wait_for_draw_to_vsync_ratio
FROM chrome_scroll_frame_info
WHERE is_janky;

-- A helper macro to generate tags for long stages in the scroll pipeline.
CREATE PERFETTO MACRO _chrome_scroll_jank_tag_long_stage(
  -- Stage (column of `chrome_scroll_frame_info`) this tag is based on.
  stage ColumnName,
  -- String tag to assign to frames that exceed the threshold.
  tag Expr,
  -- Threshold in milliseconds.
  threshold_ms Expr
)
RETURNS TableOrSubquery AS
SELECT
  id AS frame_id,
  $tag AS tag
FROM _chrome_janky_scroll_frames
WHERE $stage > time_from_ms($threshold_ms);

-- List of scroll jank causes that apply to janky scroll frames.
-- Each frame can have zero or multiple tags.
CREATE PERFETTO TABLE chrome_scroll_jank_tags(
  -- Frame ID.
  frame_id LONG,
  -- Tag of the scroll jank cause.
  tag STRING
) AS
-- Start of the the long stage tags.
-- We use 16ms as the threshold for most stages, as if they take O(vsync) or
-- more, then we have a problem.
_chrome_scroll_jank_tag_long_stage!(
  first_input_generation_to_browser_main_dur,
  'long_generation_to_browser_main',
  16
)
UNION ALL
_chrome_scroll_jank_tag_long_stage!(
  first_input_touch_move_processing_dur,
  'long_touch_move',
  16
)
UNION ALL
_chrome_scroll_jank_tag_long_stage!(
  first_input_browser_to_compositor_delay_dur,
  'long_browser_to_compositor',
  16
)
UNION ALL
_chrome_scroll_jank_tag_long_stage!(
  first_input_compositor_dispatch_dur,
  'long_compositor_dispatch',
  16
)
UNION ALL
_chrome_scroll_jank_tag_long_stage!(
  first_input_compositor_dispatch_to_on_begin_frame_delay_dur,
  'long_wait_for_begin_frame',
  16
)
UNION ALL
_chrome_scroll_jank_tag_long_stage!(
  compositor_on_begin_frame_dur,
  'long_on_begin_frame',
  16
)
UNION ALL
_chrome_scroll_jank_tag_long_stage!(
  compositor_on_begin_frame_to_generation_delay_dur,
  'long_on_begin_frame_to_generation',
  16
)
UNION ALL
_chrome_scroll_jank_tag_long_stage!(
  compositor_generate_frame_to_submit_frame_dur,
  'long_generate_to_submit',
  16
)
UNION ALL
_chrome_scroll_jank_tag_long_stage!(
  compositor_submit_frame_dur,
  'long_submit_frame',
  16
)
UNION ALL
_chrome_scroll_jank_tag_long_stage!(
  compositor_to_viz_delay_dur,
  'long_compositor_to_viz',
  16
)
UNION ALL
_chrome_scroll_jank_tag_long_stage!(
  viz_receive_compositor_frame_dur,
  'long_viz_receive_frame',
  16
)
UNION ALL
_chrome_scroll_jank_tag_long_stage!(
  viz_wait_for_draw_dur,
  'long_viz_wait_for_draw',
  16
)
UNION ALL
_chrome_scroll_jank_tag_long_stage!(
  viz_draw_and_swap_dur,
  'long_viz_draw_and_swap',
  16
)
UNION ALL
_chrome_scroll_jank_tag_long_stage!(
  viz_to_gpu_delay_dur,
  'long_viz_to_gpu',
  -- We use lower threshold, as jump to GPU should be fast.
  6
)
UNION ALL
_chrome_scroll_jank_tag_long_stage!(
  viz_swap_buffers_dur,
  'long_viz_swap_buffers',
  -- We use lower threshold, as swap buffers should be fast.
  6
)
UNION ALL
_chrome_scroll_jank_tag_long_stage!(
  viz_swap_buffers_to_latch_dur,
  'long_viz_swap_to_latch',
  -- This stage should be highly consistent and usually takes 16.6ms, so we use
  -- 17ms as the threshold.
  17
)
UNION ALL
--
-- Start of non-stage tags.
--
-- We expect at least one input event per vsync, so if the delta to the previous
-- input event is greater than 1.2 vsync intervals, we can consider this to be
-- a cause of jank.
SELECT
  id AS frame_id,
  'inconsistent_input' AS tag
FROM _chrome_janky_scroll_frames
WHERE previous_input_delta_to_vsync_ratio >= 1.2
  AND NOT is_inertial
UNION ALL
-- As we control fling generation, we want to tag inconsistencies there
-- separately.
SELECT
  id AS frame_id,
  'inconsistent_fling_input' AS tag
FROM _chrome_janky_scroll_frames
WHERE previous_input_delta_to_vsync_ratio >= 1.2
  AND is_inertial
UNION ALL
-- Having one input per frame makes us susceptible to scheduling issues, so
-- if we have a spike in the time we wait to draw a frame, consider it a scheduling
-- issue.
SELECT
  id AS frame_id,
  'infrequent_input_cc_scheduling' AS tag
FROM _chrome_janky_scroll_frames
WHERE previous_input_delta_to_vsync_ratio BETWEEN 0.75 AND 1.2
  AND wait_for_begin_frame_delta_to_vsync_ratio > 0.66
UNION ALL
SELECT
  id AS frame_id,
  'infrequent_input_viz_scheduling' AS tag
FROM _chrome_janky_scroll_frames
WHERE previous_input_delta_to_vsync_ratio BETWEEN 0.75 AND 1.2
  AND viz_wait_for_draw_to_vsync_ratio > 0.66;

-- Consolidated list of tags for each janky scroll frame.
CREATE PERFETTO TABLE chrome_tagged_janky_scroll_frames(
  -- Frame id.
  frame_id LONG,
  -- Whether this frame has any tags or not.
  tagged BOOL,
  -- Comma-separated list of tags for this frame.
  tags STRING
) AS
WITH tagged_frames AS (
  SELECT
    frame.id as frame_id,
    GROUP_CONCAT(tag ORDER BY tag) AS tags
  FROM chrome_scroll_frame_info AS frame
  LEFT JOIN chrome_scroll_jank_tags tag ON tag.frame_id = frame.id
  WHERE frame.is_janky
  GROUP BY frame_id
)
SELECT
  frame_id,
  tags IS NOT NULL AS tagged,
  tags
FROM tagged_frames;