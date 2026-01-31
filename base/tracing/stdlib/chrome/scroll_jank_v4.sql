-- Copyright 2026 The Chromium Authors
-- Use of this source code is governed by a BSD-style license that can be
-- found in the LICENSE file.

-- Results of the Scroll Jank V4 metric for frames which contain one or more
-- scroll updates.
--
-- See
-- https://docs.google.com/document/d/1AaBvTIf8i-c-WTKkjaL4vyhQMkSdynxo3XEiwpofdeA
-- and `EventLatency.ScrollJankV4Result` in
-- https://source.chromium.org/chromium/chromium/src/+/main:base/tracing/protos/chrome_track_event.proto
-- for more information.
--
-- Available since Chrome 145.0.7573.0 and cherry-picked into 144.0.7559.31.
CREATE PERFETTO TABLE chrome_scroll_jank_v4_results (
  -- Slice ID of the 'ScrollJankV4' slice.
  id ID(slice.id),
  -- Slice name ('ScrollJankV4').
  name STRING,
  -- The timestamp at the start of the slice.
  ts TIMESTAMP,
  -- The duration of the slice.
  dur DURATION,
  -- Whether this frame is janky. True if and only if there's at least one row
  -- with `id` in `chrome_scroll_jank_v4_reasons`. If true, then
  -- `vsyncs_since_previous_frame` must be greater than one.
  is_janky BOOL,
  -- How many VSyncs were between (A) this frame and (B) the previous frame.
  -- If this value is greater than one, then Chrome potentially missed one or
  -- more VSyncs (i.e. might have been able to present this scroll update
  -- earlier). NULL if this frame is the first frame in a scroll.
  vsyncs_since_previous_frame LONG,
  -- The running delivery cut-off based on frames preceding this frame. NULL
  -- if ANY of the following holds:
  --
  --   * This frame is the first frame in a scroll.
  --   * All frames since the beginning of the scroll up to and including the
  --     previous frame have been non-damaging.
  --   * The most recent janky frame was non-damaging and all frames since
  --     then up to and including the previous frame have been non-damaging.
  running_delivery_cutoff DURATION,
  -- The running delivery cut-off adjusted for this frame. NULL if ANY of the
  -- following holds:
  --
  --   * This frame is the first frame in a scroll.
  --   * This frame is non-damaging.
  --   * All frames since the beginning of the scroll up to and including the
  --     previous frame have been non-damaging.
  --   * The most recent janky frame was non-damaging and all frames since
  --     then up to and including the previous frame have been non-damaging.
  --   * `vsyncs_since_previous_frame` is equal to one.
  adjusted_delivery_cutoff DURATION,
  -- The delivery cut-off of this frame. NULL if this frame is non-damaging.
  current_delivery_cutoff DURATION,
  -- Trace ID of the first real scroll update included in this frame. Can be
  -- joined with `chrome_event_latencies.scroll_update_id`. NULL if this frame
  -- contains no real scroll updates.
  real_first_event_latency_id LONG,
  -- The actual generation timestamp of the first real scroll update included
  -- (coalesced) in this frame. NULL if this frame contains no real scroll
  -- updates.
  real_first_input_generation_ts TIMESTAMP,
  -- The actual generation timestamp of the last real scroll update included
  -- (coalesced) in this frame. NULL if this frame contains no real scroll
  -- updates.
  real_last_input_generation_ts TIMESTAMP,
  -- The absolute total raw (unpredicted) delta of all real scroll updates
  -- included in this frame (in pixels). NULL if this frame contains no real
  -- scroll updates.
  real_abs_total_raw_delta_pixels DOUBLE,
  -- The maximum absolute raw (unpredicted) delta out of all inertial (fling)
  -- scroll updates included in this frame (in pixels). NULL if there were no
  -- inertial scroll updates in this frame.
  real_max_abs_inertial_raw_delta_pixels DOUBLE,
  -- Trace ID of the first synthetic scroll update included in this frame. Can
  -- be joined with `chrome_event_latencies.scroll_update_id`. NULL if this
  -- frame contains no synthetic scroll updates.
  synthetic_first_event_latency_id LONG,
  -- The generation timestamp of the first synthetic scroll update included
  -- (coalesced) in this frame extrapolated based on the input generation →
  -- begin frame duration of the most recent real scroll update. NULL if ANY of
  -- the following holds:
  --
  --   * This frame contains no synthetic scroll updates.
  --   * This frame is janky (i.e. `is_janky` is true).
  --   * All frames since the beginning of the scroll up to and including the
  --     previous frame have contained only synthetic scroll updates.
  --   * The most recent janky frame contained only synthetic scroll updates and
  --     all frames since then up to and including the previous frame have
  --     contained only synthetic scroll updates.
  synthetic_first_extrapolated_input_generation_ts TIMESTAMP,
  -- The begin frame timestamp of the first synthetic scroll update included
  -- (coalesced) in this frame. NULL if this frame contains no synthetic scroll
  -- updates. If not NULL, it's less than or equal to `begin_frame_ts`.
  synthetic_first_original_begin_frame_ts TIMESTAMP,
  -- Type of the first scroll update in this frame. Possible values:
  --
  --   * 'REAL'
  --   * 'SYNTHETIC_WITH_EXTRAPOLATED_INPUT_GENERATION_TIMESTAMP'
  --   * 'SYNTHETIC_WITHOUT_EXTRAPOLATED_INPUT_GENERATION_TIMESTAMP'
  --
  -- The first scroll update is decided as follows:
  --
  --   * For real scroll updates, we consider their actual input generation
  --     timestamp.
  --   * For synthetic scroll updates, we extrapolate their input generation
  --     timestamp based on the input generation → begin frame duration of the
  --     most recent real scroll update UNLESS ANY of the following holds (in
  --     which case we DON'T extrapolate input generation timestamps for
  --     synthetic scroll updates in this frame):
  --       * This frame is janky.
  --       * All frames since the beginning of the scroll up to and
  --         including the previous frame have contained only synthetic
  --         scroll updates.
  --       * The most recent janky frame contained only synthetic scroll
  --         updates and all frames since then up to and including the
  --         previous frame have contained only synthetic scroll updates.
  --
  -- If, based on the above rules, the scroll update with the earliest input
  -- generation timestamp is a real scroll update, then this frame's type is
  -- 'REAL'. If the scroll update with the earliest input generation timestamp
  -- is a synthetic scroll update, then this frame's type is
  -- 'SYNTHETIC_WITH_EXTRAPOLATED_INPUT_GENERATION_TIMESTAMP'.
  --
  -- If this frame contains only synthetic scroll updates but it wasn't
  -- possible to extrapolate their input generation timestamp (for any of
  -- the reasons listed above), then this frame's type is
  -- 'SYNTHETIC_WITHOUT_EXTRAPOLATED_INPUT_GENERATION_TIMESTAMP'.
  first_scroll_update_type STRING,
  -- Trace ID of the first scroll update included in this frame.
  --
  --   * If `first_scroll_update_type` is 'REAL', then `first_event_latency_id`
  --     is equal to `real_first_event_latency_id`.
  --   * If `first_scroll_update_type` is
  --     'SYNTHETIC_WITH_EXTRAPOLATED_INPUT_GENERATION_TIMESTAMP' or
  --     'SYNTHETIC_WITHOUT_EXTRAPOLATED_INPUT_GENERATION_TIMESTAMP', then
  --     `first_event_latency_id` equal to `synthetic_first_event_latency_id`.
  --
  -- Can be joined with `chrome_event_latencies.scroll_update_id`.
  first_event_latency_id LONG,
  -- Type of scroll damage in this frame. Possible values:
  --
  --   * 'DAMAGING'
  --   * 'NON_DAMAGING_WITH_EXTRAPOLATED_PRESENTATION_TIMESTAMP'
  --   * 'NON_DAMAGING_WITHOUT_EXTRAPOLATED_PRESENTATION_TIMESTAMP'
  --
  -- A frame F is non-damaging if the following conditions are BOTH true:
  --
  --   1. All scroll updates in F are non-damaging. A scroll update is
  --      non-damaging if it didn't cause a frame update and/or didn't change
  --      the scroll offset.
  --
  --   2. All frames between (both ends exclusive):
  --        a. the last frame presented by Chrome before F and
  --        b. F
  --      are non-damaging.
  --
  -- If this frame is damaging, its type is 'DAMAGING'. If this frame is
  -- non-damaging and its presentation timestamp could be extrapolated based on
  -- the begin frame → presentation duration of the most recent damaging frame,
  -- its type is 'NON_DAMAGING_WITH_EXTRAPOLATED_PRESENTATION_TIMESTAMP'.
  --
  -- If this frame is non-damaging and its presentation timestamp couldn't be
  -- extrapolated for ANY of the reasons below, its type is
  -- 'NON_DAMAGING_WITHOUT_EXTRAPOLATED_PRESENTATION_TIMESTAMP':
  --
  --   * This frame is janky and non-damaging.
  --   * All frames since the beginning of the scroll up to and including this
  --     frame have been non-damaging.
  --   * The most recent janky frame was non-damaging and all frames since
  --     then up to and including the this frame have been non-damaging.
  --
  -- Note: The `first_scroll_update_type` and `damage_type` columns are
  -- orthogonal. The former depends on whether the frame is synthetic (only
  -- contains synthetic scroll updates). The latter depends on whether the
  -- frame is damaging. For example:
  --
  --   * If a frame is synthetic and damaging, it will[1] have an extrapolated
  --     input generation timestamp.
  --   * If a frame is real and non-damaging, it will[1] have an extrapolated
  --     presentation timestamp.
  --   * If a frame is both synthetic and damaging, it will[1] have both
  --     timestamps extrapolated.
  --
  -- [1] As long as there's Chrome past performance to extrapolate based on.
  damage_type STRING,
  -- The VSync interval that this frame was produced for according to the
  -- BeginFrameArgs.
  vsync_interval DURATION,
  -- The begin frame timestamp, at which this frame started, according to the
  -- BeginFrameArgs.
  begin_frame_ts TIMESTAMP,
  -- The presentation timestamp of the frame.
  --
  --   * If `damage_type` is 'DAMAGING', then `presentation_ts` is the actual
  --     presentation timestamp.
  --   * If `damage_type` is
  --     'NON_DAMAGING_WITH_EXTRAPOLATED_PRESENTATION_TIMESTAMP', then
  --     `presentation_ts` is an extrapolated timestamp based on the begin frame
  --     → presentation duration of the most recent damaging frame.
  --   * If `damage_type` is
  --     'NON_DAMAGING_WITHOUT_EXTRAPOLATED_PRESENTATION_TIMESTAMP', then
  --     `presentation_ts` is NULL.
  presentation_ts TIMESTAMP
) AS
WITH
  intermediate_table AS (
    SELECT
      scroll_slice.id,
      scroll_slice.name,
      scroll_slice.ts,
      scroll_slice.dur,
      extract_arg(scroll_slice.arg_set_id, 'scroll_jank_v4.is_janky') AS is_janky,
      extract_arg(scroll_slice.arg_set_id, 'scroll_jank_v4.vsyncs_since_previous_frame') AS vsyncs_since_previous_frame,
      extract_arg(scroll_slice.arg_set_id, 'scroll_jank_v4.running_delivery_cutoff_us') AS running_delivery_cutoff,
      extract_arg(scroll_slice.arg_set_id, 'scroll_jank_v4.adjusted_delivery_cutoff_us') AS adjusted_delivery_cutoff,
      extract_arg(scroll_slice.arg_set_id, 'scroll_jank_v4.current_delivery_cutoff_us') AS current_delivery_cutoff,
      extract_arg(scroll_slice.arg_set_id, 'scroll_jank_v4.updates.real.first_event_latency_id') AS real_first_event_latency_id,
      real_input_generation_slice.ts AS real_first_input_generation_ts,
      real_input_generation_slice.ts + real_input_generation_slice.dur AS real_last_input_generation_ts,
      extract_arg(scroll_slice.arg_set_id, 'scroll_jank_v4.updates.real.abs_total_raw_delta_pixels') AS real_abs_total_raw_delta_pixels,
      extract_arg(
        scroll_slice.arg_set_id,
        'scroll_jank_v4.updates.real.max_abs_inertial_raw_delta_pixels'
      ) AS real_max_abs_inertial_raw_delta_pixels,
      extract_arg(scroll_slice.arg_set_id, 'scroll_jank_v4.updates.synthetic.first_event_latency_id') AS synthetic_first_event_latency_id,
      synthetic_first_extrapolated_input_generation_slice.ts AS synthetic_first_extrapolated_input_generation_ts,
      synthetic_first_original_begin_frame_slice.ts AS synthetic_first_original_begin_frame_ts,
      extract_arg(scroll_slice.arg_set_id, 'scroll_jank_v4.updates.first_scroll_update_type') AS first_scroll_update_type,
      extract_arg(scroll_slice.arg_set_id, 'scroll_jank_v4.damage_type') AS damage_type,
      extract_arg(scroll_slice.arg_set_id, 'scroll_jank_v4.vsync_interval_us') AS vsync_interval,
      begin_frame_slice.ts AS begin_frame_ts,
      presentation_slice.ts AS presentation_ts
    FROM slice AS scroll_slice
    LEFT JOIN descendant_slice(scroll_slice.id) AS real_input_generation_slice
      ON real_input_generation_slice.name = 'Real scroll update input generation'
    LEFT JOIN descendant_slice(scroll_slice.id) AS synthetic_first_extrapolated_input_generation_slice
      ON synthetic_first_extrapolated_input_generation_slice.name = 'Extrapolated first synthetic scroll update input generation'
    LEFT JOIN descendant_slice(scroll_slice.id) AS synthetic_first_original_begin_frame_slice
      ON synthetic_first_original_begin_frame_slice.name = 'First synthetic scroll update original begin frame'
    LEFT JOIN descendant_slice(scroll_slice.id) AS begin_frame_slice
      ON begin_frame_slice.name = 'Begin frame'
    LEFT JOIN descendant_slice(scroll_slice.id) AS presentation_slice
      ON presentation_slice.name IN ('Presentation', 'Extrapolated presentation')
    WHERE
      scroll_slice.name = 'ScrollJankV4'
  )
SELECT
  id,
  name,
  ts,
  dur,
  is_janky,
  vsyncs_since_previous_frame,
  running_delivery_cutoff,
  adjusted_delivery_cutoff,
  current_delivery_cutoff,
  real_first_event_latency_id,
  real_first_input_generation_ts,
  real_last_input_generation_ts,
  real_abs_total_raw_delta_pixels,
  real_max_abs_inertial_raw_delta_pixels,
  synthetic_first_event_latency_id,
  synthetic_first_extrapolated_input_generation_ts,
  synthetic_first_original_begin_frame_ts,
  first_scroll_update_type,
  CASE first_scroll_update_type
    WHEN 'REAL'
    THEN real_first_event_latency_id
    WHEN 'SYNTHETIC_WITH_EXTRAPOLATED_INPUT_GENERATION_TIMESTAMP'
    THEN synthetic_first_event_latency_id
    WHEN 'SYNTHETIC_WITHOUT_EXTRAPOLATED_INPUT_GENERATION_TIMESTAMP'
    THEN synthetic_first_event_latency_id
    ELSE coalesce(real_first_event_latency_id, synthetic_first_event_latency_id)
  END AS first_event_latency_id,
  damage_type,
  vsync_interval,
  begin_frame_ts,
  presentation_ts
FROM intermediate_table
ORDER BY
  ts ASC;

-- Reasons why the Scroll Jank V4 metric marked frames as janky.
--
-- A frame might be janky for multiple reasons, so this table might contain
-- multiple rows with the same `id` and distinct `jank_reason`s.
--
-- Available since Chrome 145.0.7573.0 and cherry-picked into 144.0.7559.31.
CREATE PERFETTO TABLE chrome_scroll_jank_v4_reasons (
  -- Slice ID of the 'ScrollJankV4' slice. Can be joined with
  -- `chrome_scroll_jank_v4_results.id`.
  id JOINID(slice.id),
  -- A reason why the frame is janky. Possible values:
  --
  --   * 'MISSED_VSYNC_DUE_TO_DECELERATING_INPUT_FRAME_DELIVERY': Chrome's
  --     input→frame delivery slowed down to the point that it missed one or
  --     more VSyncs.
  --   * 'MISSED_VSYNC_DURING_FAST_SCROLL': Chrome missed one or more VSyncs in
  --     the middle of a fast regular scroll.
  --   * 'MISSED_VSYNC_AT_START_OF_FLING': Chrome missed one or more VSyncs
  --     during the transition from a fast regular scroll to a fling.
  --   * 'MISSED_VSYNC_DURING_FLING': Chrome missed one or more VSyncs in the
  --     middle of a fling.
  jank_reason STRING,
  -- Number of VSyncs that that Chrome missed (for `jank_reason`) before
  -- presenting the first scroll update in the frame. Greater than zero.
  missed_vsyncs LONG
) AS
WITH
  -- Find all 'scroll_jank_v4.missed_vsyncs_per_jank_reason[N]' argument key
  -- prefixes.
  key_prefixes_with_indices AS (
    SELECT DISTINCT
      slice.id,
      arg_set_id,
      substr(args.key, 1, instr(args.key, ']')) AS key_prefix_with_index
    FROM slice
    JOIN args
      USING (arg_set_id)
    WHERE
      slice.name = 'ScrollJankV4'
      -- "[...]" represents a range in a glob pattern, so we must escape "[" as
      -- "[[]" and "]" as "[]]".
      AND args.key GLOB 'scroll_jank_v4.missed_vsyncs_per_jank_reason[[]*[]].*'
  )
SELECT
  id,
  extract_arg(arg_set_id, key_prefix_with_index || '.jank_reason') AS jank_reason,
  extract_arg(arg_set_id, key_prefix_with_index || '.missed_vsyncs') AS missed_vsyncs
FROM key_prefixes_with_indices;
