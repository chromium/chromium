-- Copyright 2025 The Chromium Authors
-- Use of this source code is governed by a BSD-style license that can be
-- found in the LICENSE file.

-- LoadLine 2 benchmark stage, renderer subsystem, frame presentation, and
-- JavaScript lifecycle tables.
-- Required trace categories: blink.user_timing, navigation, loading, v8,
-- devtools.timeline, disabled-by-default-v8.compile, cc, benchmark, toplevel,
-- blink (plus sched for 'runnable' vs 'idle_wait' thread_state attribution).
-- Assumes each story runs once per trace and analyses the renderer process
-- that emits the story's finish mark.

INCLUDE PERFETTO MODULE intervals.intersect;
INCLUDE PERFETTO MODULE slices.with_context;

-- Gets the presentation time of the first frame committed after ts
-- in the renderer process with pid.
CREATE PERFETTO FUNCTION _chrome_get_next_presentation_time_by_pid(
    ts TIMESTAMP, pid LONG)
RETURNS TIMESTAMP
AS
SELECT MIN(a.ts + a.dur) AS ts
FROM process_slice s, ancestor_slice(s.id) a
WHERE
  s.name = 'Commit'
  AND a.name = 'PipelineReporter'
  AND s.depth - 1 = a.depth
  AND s.ts > $ts
  AND s.pid = $pid
  -- TODO(crbug.com/409484302): Once we are no longer interested in Chrome
  -- versions <=M136, leave only 'frame_reporter'.
  AND COALESCE(
        EXTRACT_ARG(a.arg_set_id, 'frame_reporter.state'),
        EXTRACT_ARG(a.arg_set_id, 'chrome_frame_reporter.state')
      ) = 'STATE_PRESENTED_ALL';

-- User timing trace events can be emitted by either performance.mark() or
-- performance.measure(). The former appear on the CrRendererMain thread track,
-- the latter on their own custom track inside the Renderer process.
-- This query looks for the event track info in both thread_track and
-- process_track to support both cases.
CREATE PERFETTO VIEW _chrome_loadline2_marks_with_pid (
  -- Mark timestamp
  ts TIMESTAMP,
  -- Name of the page
  page STRING,
  -- Name of the mark
  mark STRING,
  -- PID of the Renderer process
  pid LONG
) AS
SELECT
  ts,
  STR_SPLIT(s.name, '/', 1) AS page,
  STR_SPLIT(s.name, '/', 2) AS mark,
  pid
FROM slice s
LEFT JOIN thread_track tt ON s.track_id = tt.id
LEFT JOIN process_track pt ON s.track_id = pt.id
LEFT JOIN thread t ON tt.utid = t.utid
JOIN process p ON p.upid = COALESCE(t.upid, pt.upid)
WHERE s.category = 'blink.user_timing' AND s.name GLOB 'LoadLine2/*/*';

-- Story start for each page
CREATE PERFETTO TABLE _chrome_loadline2_story_start (
  -- Name of the page
  page STRING,
  -- Story start timestamp
  story_start TIMESTAMP
) AS
SELECT
  page,
  ts AS story_start
FROM _chrome_loadline2_marks_with_pid
WHERE mark = 'start';

-- Renderer process pid for each page
CREATE PERFETTO TABLE _chrome_loadline2_story_pid (
  -- Name of the page
  page STRING,
  -- PID of the Renderer process
  pid LONG
) AS
SELECT
  page,
  pid
FROM _chrome_loadline2_marks_with_pid
WHERE mark = 'finish';

-- Story start and Renderer pid for each page
CREATE PERFETTO TABLE _chrome_loadline2_story_start_with_pid (
  -- Name of the page
  page STRING,
  -- Story start timestamp
  story_start TIMESTAMP,
  -- PID of the Renderer process
  pid LONG
) AS
SELECT
  page,
  story_start,
  pid
FROM _chrome_loadline2_story_start JOIN _chrome_loadline2_story_pid USING (page);

-- Start timestamp for the first network request for each page
CREATE PERFETTO TABLE _chrome_loadline2_start_request (
  -- Name of the page
  page STRING,
  -- Start request timestamp
  start_request TIMESTAMP
) AS
SELECT
  page,
  MIN(ts) AS start_request
FROM slice, _chrome_loadline2_story_start_with_pid
WHERE
  name = 'WillStartRequest'
  AND ts >= story_start
GROUP BY page;

-- Finish timestamp for the first network request for each page
CREATE PERFETTO TABLE _chrome_loadline2_end_request (
  -- Name of the page
  page STRING,
  -- End request timestamp
  end_request TIMESTAMP
) AS
SELECT
  page,
  MIN(ts) AS end_request
FROM slice, _chrome_loadline2_story_start_with_pid
WHERE
  name = 'CommitSentToFirstSubresourceLoadStart'
  AND ts >= story_start
GROUP BY page;

-- Renderer ready for each page
CREATE PERFETTO TABLE _chrome_loadline2_renderer_ready (
  -- Name of the page
  page STRING,
  -- Renderer ready timestamp
  renderer_ready TIMESTAMP
) AS
SELECT
  page,
  MIN(ts) AS renderer_ready
FROM thread_slice
JOIN _chrome_loadline2_story_start_with_pid USING (pid)
WHERE
  name = 'DocumentLoader::CommitNavigation'
  AND ts >= story_start
GROUP BY page;

-- Visual mark for each page
CREATE PERFETTO TABLE _chrome_loadline2_visual_mark (
  -- Name of the page
  page STRING,
  -- Visual mark timestamp
  visual_mark TIMESTAMP,
  -- PID of the Renderer process
  pid LONG
) AS
SELECT
  page,
  ts AS visual_mark,
  pid
FROM _chrome_loadline2_marks_with_pid
WHERE mark = 'visual';

-- Timestamp of the second rAF after visual mark for each page
CREATE PERFETTO TABLE _chrome_loadline2_visual_raf (
  -- Name of the page
  page STRING,
  -- Visual raf timestamp
  visual_raf TIMESTAMP
) AS
SELECT
  page,
  ts AS visual_raf
FROM _chrome_loadline2_marks_with_pid
WHERE mark = 'visual_raf';

-- Visual presentation for each page
CREATE PERFETTO TABLE _chrome_loadline2_visual_presentation (
  -- Name of the page
  page STRING,
  -- Visual presentation timestamp
  visual_presentation TIMESTAMP
) AS
SELECT
  page,
  _chrome_get_next_presentation_time_by_pid(visual_mark, pid) AS visual_presentation
FROM _chrome_loadline2_visual_mark;

-- Interactive mark for each page
CREATE PERFETTO TABLE _chrome_loadline2_interactive_mark (
  -- Name of the page
  page STRING,
  -- Interactive mark timestamp
  interactive_mark TIMESTAMP,
  -- PID of the Renderer process
  pid LONG
) AS
SELECT
  page,
  ts AS interactive_mark,
  pid
FROM _chrome_loadline2_marks_with_pid
WHERE mark = 'interactive';

-- Timestamp of the second rAF after interactive mark for each page
CREATE PERFETTO TABLE _chrome_loadline2_interactive_raf (
  -- Name of the page
  page STRING,
  -- Interactive raf timestamp
  interactive_raf TIMESTAMP
) AS
SELECT
  page,
  ts AS interactive_raf
FROM _chrome_loadline2_marks_with_pid
WHERE mark = 'interactive_raf';

-- Interactive presentation for each page
CREATE PERFETTO TABLE _chrome_loadline2_interactive_presentation (
  -- Name of the page
  page STRING,
  -- Interactive presentation timestamp
  interactive_presentation TIMESTAMP
) AS
SELECT
  page,
  _chrome_get_next_presentation_time_by_pid(interactive_mark, pid) AS interactive_presentation
FROM _chrome_loadline2_interactive_mark;

-- Story finish for each page
CREATE PERFETTO TABLE _chrome_loadline2_story_finish (
  -- Name of the page
  page STRING,
  -- Story finish timestamp
  story_finish TIMESTAMP
) AS
SELECT
  page,
  ts AS story_finish
FROM _chrome_loadline2_marks_with_pid
WHERE mark = 'finish';

-- All LoadLine2 stages per page
CREATE PERFETTO TABLE chrome_loadline2_stages (
  -- Name of the page
  page STRING,
  -- Story start timestamp
  story_start TIMESTAMP,
  -- Start request timestamp
  start_request TIMESTAMP,
  -- End request timestamp
  end_request TIMESTAMP,
  -- Renderer ready timestamp
  renderer_ready TIMESTAMP,
  -- Visual mark timestamp
  visual_mark TIMESTAMP,
  -- Visual rAF timestamp
  visual_raf TIMESTAMP,
  -- Visual presentation timestamp
  visual_presentation TIMESTAMP,
  -- Interactive mark timestamp
  interactive_mark TIMESTAMP,
  -- Interactive rAF timestamp
  interactive_raf TIMESTAMP,
  -- Interactive presentation timestamp
  interactive_presentation TIMESTAMP,
  -- Story finish timestamp
  story_finish TIMESTAMP
) AS
SELECT
  page,
  story_start,
  start_request,
  end_request,
  renderer_ready,
  visual_mark,
  visual_raf,
  visual_presentation,
  interactive_mark,
  interactive_raf,
  interactive_presentation,
  story_finish
FROM _chrome_loadline2_story_start_with_pid
LEFT JOIN _chrome_loadline2_start_request USING (page)
LEFT JOIN _chrome_loadline2_end_request USING (page)
LEFT JOIN _chrome_loadline2_renderer_ready USING (page)
LEFT JOIN _chrome_loadline2_visual_mark USING (page)
LEFT JOIN _chrome_loadline2_visual_raf USING (page)
LEFT JOIN _chrome_loadline2_visual_presentation USING (page)
LEFT JOIN _chrome_loadline2_interactive_mark USING (page)
LEFT JOIN _chrome_loadline2_interactive_raf USING (page)
LEFT JOIN _chrome_loadline2_interactive_presentation USING (page)
LEFT JOIN _chrome_loadline2_story_finish USING (page);

-- Classifies a CrRendererMain slice into a LoadLine 2 subsystem.
CREATE PERFETTO MACRO _chrome_loadline2_classify_renderer_slice(
    -- Slice category expression.
    category Expr,
    -- Slice name expression.
    name Expr,
    -- Slice task.posted_from.file_name argument expression.
    posted_from_file Expr
)
RETURNS Expr
AS
CASE
  WHEN $category GLOB '*gc*'
    OR $name GLOB '*GC*'
    OR $name GLOB 'Oilpan*'
    OR $name GLOB 'CppGC*'
    OR $posted_from_file GLOB '*v8/src/heap/*'
    OR $posted_from_file GLOB '*cppgc*'
    THEN 'gc'
  WHEN $name = 'v8.streamingCompile'
    OR $name GLOB 'v8.compile*'
    OR $name GLOB 'V8.Compile*'
    OR $name GLOB 'v8.parse*'
    OR $name GLOB 'V8.Parse*'
    OR $name GLOB 'v8.produce*Cache*'
    OR $name GLOB 'v8.deserialize*'
    OR $name GLOB 'V8.Deserialize*'
    OR $name = 'V8.SnapshotDecompress'
    OR $name GLOB 'v8.wasm*'
    OR $name GLOB 'V8.Wasm*'
    THEN 'js_compile'
  WHEN $category GLOB '*v8*'
    OR $name IN (
      'EvaluateScript',
      'v8.callFunction',
      'v8.run',
      'v8.evaluateModule',
      'V8.Execute',
      'BlinkScheduler_PerformMicrotaskCheckpoint'
    )
    OR $posted_from_file GLOB '*core/script/*'
    THEN 'js_execute'
  WHEN $name GLOB '*Style*'
    OR $name GLOB '*Layout*'
    OR $name GLOB '*layout*'
    OR $category GLOB '*style*'
    OR $category GLOB '*layout*'
    OR $posted_from_file GLOB '*core/css/*'
    OR $posted_from_file GLOB '*core/style/*'
    OR $posted_from_file GLOB '*core/layout/*'
    OR $posted_from_file GLOB '*html_style_element*'
    THEN 'style_layout'
  WHEN $name GLOB '*Paint*'
    OR $name GLOB '*PrePaint*'
    OR $name GLOB '*Composite*'
    OR $name GLOB '*Compositing*'
    OR $name GLOB '*LayerTree*'
    OR $name GLOB '*BeginMainFrame*'
    OR $name IN (
      'CullRectUpdate',
      'LocalFrameView::UpdateViewportIntersectionsForSubtree',
      'GpuImageDecodeCache::DecodeImage',
      'ImageFrameGenerator::decode'
    )
    OR ($name GLOB '*Commit*' AND $category GLOB '*cc*')
    OR $posted_from_file GLOB '*cc/trees/*'
    OR $posted_from_file GLOB '*core/paint/*'
    THEN 'paint_compositing'
  WHEN $name IN (
      'DecodedDataDocumentParser::AppendBytes',
      'DocumentLoader::CommitData',
      'DocumentLoader::HandleData',
      'DocumentLoader::BodyDataReceivedImpl',
      'DocumentLoader::BodyLoadingFinished'
    )
    OR $name GLOB 'NavigationBodyLoader::*'
    OR $name GLOB 'HTMLDocumentParser*'
    OR $name GLOB '*ParseHTML*'
    OR $posted_from_file GLOB '*html/parser/*'
    THEN 'html_parse'
  WHEN $name != 'RenderFrameImpl::DidObserveNewFeatureUsage'
    AND (
      $category GLOB '*navigation*'
      OR $name GLOB 'DocumentLoader::*'
      OR $name GLOB 'RenderFrameImpl::*'
      OR $name GLOB 'FrameLoader::*'
      OR $name GLOB 'HTMLFrameOwnerElement::*'
      OR $name GLOB 'RemoteFrame::*'
      OR $name GLOB 'AgentSchedulingGroup::*'
      OR $name GLOB 'LocalFrame::*'
    )
    THEN 'navigation_commit'
  ELSE NULL
END;

-- Maps each LoadLine 2 story to its CrRendererMain thread utid and stage
-- bounds, selecting the most recent CrRendererMain thread active during the
-- story window when multiple process rows share pid or an OS pid is reused.
CREATE PERFETTO TABLE _chrome_loadline2_renderer_main_utid AS
SELECT
  s.page,
  MAX(t.utid) AS utid,
  MAX(
    COALESCE(s.renderer_ready, s.end_request),
    COALESCE(s.end_request, s.renderer_ready)
  ) AS renderer_start,
  s.story_finish
FROM chrome_loadline2_stages s
JOIN _chrome_loadline2_story_pid sp USING (page)
JOIN process p ON p.pid = sp.pid
JOIN thread t ON t.upid = p.upid
  AND t.name = 'CrRendererMain'
  AND (t.start_ts IS NULL OR t.start_ts <= s.story_finish)
  AND (t.end_ts IS NULL OR t.end_ts >= s.story_start)
GROUP BY s.page, s.renderer_ready, s.end_request, s.story_finish;

-- Clips CrRendererMain thread slices to the story window and assigns each
-- slice a subsystem, inheriting from parent/ancestor slices when unclassified.
CREATE PERFETTO TABLE _chrome_loadline2_cr_main_slices AS
WITH
  slices_with_args AS MATERIALIZED (
    SELECT
      su.page,
      su.utid,
      su.renderer_start,
      su.story_finish,
      ts.id,
      ts.parent_id,
      ts.depth,
      ts.category,
      ts.name,
      MAX(ts.ts, su.renderer_start) AS ts,
      MIN(ts.ts + ts.dur, su.story_finish) -
        MAX(ts.ts, su.renderer_start) AS dur,
      EXTRACT_ARG(ts.arg_set_id, 'task.posted_from.file_name')
        AS posted_from_file
    FROM _chrome_loadline2_renderer_main_utid su
    JOIN thread_track tt ON tt.utid = su.utid
      AND tt.name IS NULL
      AND tt.type = 'thread_track_event'
    JOIN slice ts ON ts.track_id = tt.id
      AND ts.ts + ts.dur > su.renderer_start
      AND ts.ts < su.story_finish
      AND ts.dur > 0
  ),
  raw_slices AS MATERIALIZED (
    SELECT
      page,
      utid,
      renderer_start,
      story_finish,
      id,
      parent_id,
      depth,
      ts,
      dur,
      _chrome_loadline2_classify_renderer_slice!(
        category, name, posted_from_file
      ) AS self_subsystem
    FROM slices_with_args
  )
SELECT
  rs.page,
  rs.utid,
  rs.renderer_start,
  rs.story_finish,
  rs.id,
  p1.id AS parent_id,
  rs.depth,
  rs.ts,
  rs.dur,
  COALESCE(
    rs.self_subsystem,
    CASE
      WHEN rs.depth = 0 THEN 'other_busy'
      WHEN rs.depth = 1 THEN COALESCE(p1.self_subsystem, 'other_busy')
      ELSE COALESCE(
        p1.self_subsystem,
        (
          SELECT anc_rs.self_subsystem
          FROM ancestor_slice(rs.parent_id) anc
          JOIN raw_slices anc_rs
            ON anc_rs.page = rs.page AND anc_rs.id = anc.id
          WHERE anc_rs.self_subsystem IS NOT NULL
          ORDER BY anc.depth DESC
          LIMIT 1
        ),
        'other_busy'
      )
    END
  ) AS subsystem
FROM raw_slices rs
LEFT JOIN raw_slices p1 ON p1.page = rs.page AND p1.id = rs.parent_id;

CREATE PERFETTO INDEX _chrome_loadline2_cr_main_slices_id
  ON _chrome_loadline2_cr_main_slices(page, id);

-- Flattens nested CrRendererMain slices into non-overlapping atomic intervals
-- using a sweep line where slice starts (1000 + depth) beat slice ends
-- (-depth) and window boundaries (-1000), and shallower parent slice ends (-d)
-- beat deeper child slice ends (-(d + 1)) at the same timestamp.
CREATE PERFETTO TABLE _chrome_loadline2_cr_raw_intervals AS
WITH
  events AS (
    SELECT
      page,
      utid,
      renderer_start AS ts,
      NULL AS active_id,
      -1000 AS priority
    FROM _chrome_loadline2_renderer_main_utid
    UNION ALL
    SELECT
      page,
      utid,
      story_finish AS ts,
      NULL AS active_id,
      -1000 AS priority
    FROM _chrome_loadline2_renderer_main_utid
    UNION ALL
    SELECT page, utid, ts, id AS active_id, 1000 + depth AS priority
    FROM _chrome_loadline2_cr_main_slices
    WHERE dur > 0
    UNION ALL
    SELECT
      page,
      utid,
      ts + dur AS ts,
      parent_id AS active_id,
      -depth AS priority
    FROM _chrome_loadline2_cr_main_slices
    WHERE dur > 0
  ),
  deduped_ranked AS (
    SELECT
      page,
      utid,
      ts,
      active_id,
      ROW_NUMBER() OVER (
        PARTITION BY page, ts
        ORDER BY priority DESC, active_id DESC
      ) AS rn
    FROM events
  ),
  deduped AS (
    SELECT page, utid, ts, active_id
    FROM deduped_ranked
    WHERE rn = 1
  ),
  atomic_intervals AS (
    SELECT
      page,
      utid,
      ts,
      LEAD(ts) OVER (PARTITION BY page ORDER BY ts) - ts AS dur,
      active_id
    FROM deduped
  )
SELECT
  ROW_NUMBER() OVER (ORDER BY a.page, a.ts) AS id,
  a.page,
  a.utid,
  a.ts,
  a.dur,
  a.active_id,
  CASE
    WHEN a.active_id IS NOT NULL THEN COALESCE(c.subsystem, 'other_busy')
    ELSE NULL
  END AS subsystem
FROM atomic_intervals a
LEFT JOIN _chrome_loadline2_cr_main_slices c
  ON c.page = a.page AND c.id = a.active_id
WHERE a.dur > 0;

-- Uncovered intervals on CrRendererMain where no trace slice is active.
CREATE PERFETTO TABLE _chrome_loadline2_cr_gaps AS
SELECT id, page, utid, ts, dur
FROM _chrome_loadline2_cr_raw_intervals
WHERE active_id IS NULL;

-- Classifies CrRendererMain scheduler thread_state intervals into 'runnable',
-- 'other_busy', or 'idle_wait', filling any sub-intervals not covered by
-- thread_state (when sched is absent or partially recorded) with 'idle_wait'.
CREATE PERFETTO TABLE _chrome_loadline2_cr_thread_state AS
WITH
  real_states AS (
    SELECT
      su.page,
      su.utid,
      su.renderer_start,
      su.story_finish,
      MAX(ts.ts, su.renderer_start) AS ts,
      MIN(ts.ts + ts.dur, su.story_finish) -
        MAX(ts.ts, su.renderer_start) AS dur,
      CASE
        WHEN ts.state IN ('R', 'R+') THEN 'runnable'
        WHEN ts.state = 'Running' THEN 'other_busy'
        ELSE 'idle_wait'
      END AS subsystem
    FROM _chrome_loadline2_renderer_main_utid su
    JOIN thread_state ts ON ts.utid = su.utid
      AND ts.ts + ts.dur > su.renderer_start
      AND ts.ts < su.story_finish
      AND ts.dur > 0
  ),
  real_states_with_prev AS (
    SELECT
      page,
      utid,
      ts,
      dur,
      subsystem,
      COALESCE(
        MAX(ts + dur) OVER (
          PARTITION BY page
          ORDER BY ts
          ROWS BETWEEN UNBOUNDED PRECEDING AND 1 PRECEDING
        ),
        renderer_start
      ) AS prev_end_ts
    FROM real_states
    WHERE dur > 0
  ),
  state_gaps AS (
    SELECT
      page,
      utid,
      prev_end_ts AS ts,
      ts - prev_end_ts AS dur,
      'idle_wait' AS subsystem
    FROM real_states_with_prev
    WHERE ts > prev_end_ts
    UNION ALL
    SELECT
      su.page,
      su.utid,
      COALESCE(MAX(rs.ts + rs.dur), su.renderer_start) AS ts,
      su.story_finish -
        COALESCE(MAX(rs.ts + rs.dur), su.renderer_start) AS dur,
      'idle_wait' AS subsystem
    FROM _chrome_loadline2_renderer_main_utid su
    LEFT JOIN real_states rs ON rs.page = su.page AND rs.dur > 0
    GROUP BY su.page, su.utid, su.renderer_start, su.story_finish
    HAVING su.story_finish > COALESCE(MAX(rs.ts + rs.dur), su.renderer_start)
  ),
  all_states AS (
    SELECT page, utid, ts, dur, subsystem
    FROM real_states
    WHERE dur > 0
    UNION ALL
    SELECT page, utid, ts, dur, subsystem
    FROM state_gaps
    WHERE dur > 0
  )
SELECT
  ROW_NUMBER() OVER (ORDER BY page, ts) AS id,
  page,
  utid,
  ts,
  dur,
  subsystem
FROM all_states;

CREATE PERFETTO INDEX _chrome_loadline2_cr_gaps_id
  ON _chrome_loadline2_cr_gaps(id);

CREATE PERFETTO INDEX _chrome_loadline2_cr_thread_state_id
  ON _chrome_loadline2_cr_thread_state(id);

-- Non-overlapping flattened activity slices on CrRendererMain for each
-- LoadLine 2 story.
CREATE PERFETTO TABLE chrome_loadline2_renderer_slices (
  -- Unique slice row identifier
  id LONG,
  -- Name of the page
  page STRING,
  -- Interval start timestamp
  ts TIMESTAMP,
  -- Interval duration
  dur DURATION,
  -- Renderer main-thread activity category ('html_parse', 'js_compile',
  -- 'js_execute', 'style_layout', 'paint_compositing', 'navigation_commit',
  -- 'gc', 'other_busy', 'runnable', or 'idle_wait'; uncovered time falls back
  -- to 'idle_wait' when scheduler thread_state data is absent)
  subsystem STRING
) AS
WITH
  combined AS (
    SELECT page, ts, dur, subsystem
    FROM _chrome_loadline2_cr_raw_intervals
    WHERE active_id IS NOT NULL
    UNION ALL
    SELECT
      g.page,
      ii.ts,
      ii.dur,
      ts.subsystem
    FROM _interval_intersect!(
      (_chrome_loadline2_cr_gaps, _chrome_loadline2_cr_thread_state),
      (utid)
    ) ii
    JOIN _chrome_loadline2_cr_gaps g ON g.id = ii.id_0
    JOIN _chrome_loadline2_cr_thread_state ts
      ON ts.id = ii.id_1 AND ts.page = g.page
    WHERE ii.dur > 0
  ),
  marked AS (
    SELECT
      page,
      ts,
      dur,
      subsystem,
      CASE
        WHEN LAG(subsystem) OVER (PARTITION BY page ORDER BY ts) = subsystem
          AND LAG(ts + dur) OVER (PARTITION BY page ORDER BY ts) = ts
        THEN 0
        ELSE 1
      END AS is_new_group
    FROM combined
  ),
  grouped AS (
    SELECT
      page,
      ts,
      dur,
      subsystem,
      SUM(is_new_group) OVER (PARTITION BY page ORDER BY ts) AS group_id
    FROM marked
  )
SELECT
  ROW_NUMBER() OVER (ORDER BY page, MIN(ts)) AS id,
  page,
  MIN(ts) AS ts,
  MAX(ts + dur) - MIN(ts) AS dur,
  subsystem
FROM grouped
GROUP BY page, group_id, subsystem
ORDER BY page, ts;

-- Breakdown of the LoadLine 2 renderer stage by CrRendererMain subsystem.
CREATE PERFETTO TABLE chrome_loadline2_renderer_breakdown (
  -- Name of the page
  page STRING,
  -- Metric name ('visual' or 'interactive')
  metric STRING,
  -- Renderer main-thread activity category ('html_parse', 'js_compile',
  -- 'js_execute', 'style_layout', 'paint_compositing', 'navigation_commit',
  -- 'gc', 'other_busy', 'runnable', or 'idle_wait')
  subsystem STRING,
  -- Total duration in the renderer stage
  dur DURATION,
  -- Percentage of the renderer stage duration
  percentage DOUBLE
) AS
WITH
  stage_bounds AS (
    SELECT
      page,
      'visual' AS metric,
      MAX(
        COALESCE(renderer_ready, end_request),
        COALESCE(end_request, renderer_ready)
      ) AS start_ts,
      visual_mark AS end_ts
    FROM chrome_loadline2_stages
    UNION ALL
    SELECT
      page,
      'interactive' AS metric,
      MAX(
        COALESCE(renderer_ready, end_request),
        COALESCE(end_request, renderer_ready)
      ) AS start_ts,
      interactive_mark AS end_ts
    FROM chrome_loadline2_stages
  ),
  clipped AS (
    SELECT
      sb.page,
      sb.metric,
      sb.end_ts - sb.start_ts AS stage_dur,
      rs.subsystem,
      MIN(rs.ts + rs.dur, sb.end_ts) - MAX(rs.ts, sb.start_ts) AS dur
    FROM stage_bounds sb
    JOIN chrome_loadline2_renderer_slices rs ON rs.page = sb.page
      AND rs.ts + rs.dur > sb.start_ts
      AND rs.ts < sb.end_ts
  )
SELECT
  page,
  metric,
  subsystem,
  SUM(dur) AS dur,
  100.0 * SUM(dur) / MAX(stage_dur) AS percentage
FROM clipped
WHERE dur > 0
GROUP BY page, metric, subsystem
ORDER BY page, metric, dur DESC;

-- Frame pipeline (PipelineReporter) sub-stage slices during the presentation
-- stage.
CREATE PERFETTO TABLE chrome_loadline2_presentation_breakdown (
  -- Unique row identifier
  id LONG,
  -- Name of the page
  page STRING,
  -- Metric name ('visual' or 'interactive')
  metric STRING,
  -- Relative depth in the PipelineReporter hierarchy (1 or 2)
  stage_depth LONG,
  -- Frame pipeline sub-stage name
  stage STRING,
  -- Start timestamp clamped to the presentation stage
  ts TIMESTAMP,
  -- Duration clamped to the presentation stage
  dur DURATION
) AS
WITH
  -- Presentation stage bounds [mark_ts, pres_ts] per story and metric.
  marks AS (
    SELECT
      page,
      'visual' AS metric,
      visual_mark AS mark_ts,
      visual_presentation AS pres_ts,
      pid
    FROM chrome_loadline2_stages
    JOIN _chrome_loadline2_story_pid USING (page)
    UNION ALL
    SELECT
      page,
      'interactive' AS metric,
      interactive_mark AS mark_ts,
      interactive_presentation AS pres_ts,
      pid
    FROM chrome_loadline2_stages
    JOIN _chrome_loadline2_story_pid USING (page)
  ),
  -- Finds the earliest presented PipelineReporter whose Commit starts after
  -- mark_ts.
  winning_pr AS (
    SELECT
      m.page,
      m.metric,
      m.mark_ts,
      m.pres_ts,
      a.id AS pr_id,
      a.depth AS pr_depth,
      ROW_NUMBER() OVER (
        PARTITION BY m.page, m.metric
        ORDER BY a.ts + a.dur ASC, s.ts ASC, a.ts ASC, a.id ASC
      ) AS rn
    FROM marks m
    JOIN process_slice s
      ON s.pid = m.pid
      AND s.name = 'Commit'
      AND s.ts > m.mark_ts
      AND s.ts <= m.pres_ts
    JOIN ancestor_slice(s.id) a
      ON a.name = 'PipelineReporter' AND s.depth - 1 = a.depth
    WHERE COALESCE(
      EXTRACT_ARG(a.arg_set_id, 'frame_reporter.state'),
      EXTRACT_ARG(a.arg_set_id, 'chrome_frame_reporter.state')
    ) = 'STATE_PRESENTED_ALL'
  ),
  -- Extracts depth-1 and depth-2 sub-stages of the winning PipelineReporter.
  pr_descendants AS (
    SELECT
      w.page,
      w.metric,
      w.mark_ts,
      w.pres_ts,
      d.depth - w.pr_depth AS stage_depth,
      d.name AS stage,
      d.ts,
      d.dur
    FROM winning_pr w
    JOIN descendant_slice(w.pr_id) d
    WHERE w.rn = 1 AND d.dur > 0 AND d.depth - w.pr_depth <= 2
  ),
  -- Earliest depth-1 stage start timestamp, used to synthesize
  -- WaitForBeginImplFrame when the PipelineReporter starts after mark_ts.
  first_child AS (
    SELECT
      page,
      metric,
      mark_ts,
      MIN(ts) AS first_ts
    FROM pr_descendants
    WHERE stage_depth = 1
    GROUP BY page, metric, mark_ts
  ),
  -- Combines any leading WaitForBeginImplFrame gap with clamped sub-stages.
  all_stages AS (
    SELECT
      page,
      metric,
      1 AS stage_depth,
      'WaitForBeginImplFrame' AS stage,
      mark_ts AS ts,
      first_ts - mark_ts AS dur
    FROM first_child
    WHERE first_ts > mark_ts
    UNION ALL
    SELECT
      page,
      metric,
      stage_depth,
      stage,
      MAX(ts, mark_ts) AS ts,
      MIN(ts + dur, pres_ts) - MAX(ts, mark_ts) AS dur
    FROM pr_descendants
    WHERE ts + dur > mark_ts AND ts < pres_ts
  )
SELECT
  ROW_NUMBER() OVER (ORDER BY page, metric, ts, stage_depth, stage) AS id,
  page,
  metric,
  stage_depth,
  stage,
  ts,
  dur
FROM all_stages
WHERE dur > 0
ORDER BY page, metric, ts, stage_depth, stage;

-- End-to-end JS script lifecycle per page (request -> response -> bg_parse ->
-- compile -> execute).
CREATE PERFETTO TABLE chrome_loadline2_js_scripts (
  -- Unique script identifier
  script_id LONG,
  -- Name of the page
  page STRING,
  -- Heuristic human-readable script or bundle name derived from the URL
  script_name STRING,
  -- Script URL (or document URL for inline scripts when available, else NULL)
  url STRING,
  -- Script classification ('streamed', 'external_non_streamed', 'module',
  -- 'preloaded_unused', 'inline')
  script_kind STRING,
  -- True if inline script, false if external script
  is_inline BOOL,
  -- True if parsed on background streaming thread, false otherwise
  is_streamed BOOL,
  -- Reason why script was not streamed (or 'preloaded (never executed)' for
  -- unused preloads), or NULL if background-streamed and executed
  not_streamed_reason STRING,
  -- Thread where compilation/execution (or background parse for unused
  -- preloads) occurred
  thread_name STRING,
  -- Execution timing relative to visual_mark and interactive_mark
  -- ('before_visual', 'spanning_visual', 'before_interactive',
  -- 'spanning_interactive', 'after_interactive', 'never_executed', or NULL if
  -- marks are missing)
  milestone_status STRING,
  -- Network request start timestamp, or NULL for inline scripts
  request_start_ts TIMESTAMP,
  -- Network response headers received timestamp, or NULL for inline scripts
  response_start_ts TIMESTAMP,
  -- Network response completion timestamp, or NULL for inline scripts
  response_end_ts TIMESTAMP,
  -- Network request-to-response-start duration, or NULL for inline scripts
  request_dur DURATION,
  -- Network response body transfer duration, or NULL for inline scripts
  response_dur DURATION,
  -- Background streaming parse start timestamp, or NULL if not streamed
  bg_parse_start_ts TIMESTAMP,
  -- Background streaming parse end timestamp, or NULL if not streamed
  bg_parse_end_ts TIMESTAMP,
  -- Total wall duration of background streaming parse, or NULL if not streamed
  bg_parse_dur DURATION,
  -- V8 background parser active parsing duration (v8.parseOnBackgroundParsing),
  -- or NULL if not streamed
  bg_parse_cpu_dur DURATION,
  -- V8 background parser stream wait duration (v8.parseOnBackgroundWaiting),
  -- or NULL if not streamed
  bg_parse_wait_dur DURATION,
  -- Main-thread compilation start timestamp, or NULL if never compiled
  compile_ts TIMESTAMP,
  -- Main-thread compilation duration, or NULL if never compiled
  compile_dur DURATION,
  -- Script execution start timestamp, or NULL if never executed
  execute_ts TIMESTAMP,
  -- Script execution duration, or NULL if never executed
  execute_dur DURATION,
  -- Total end-to-end duration from earliest stage start to latest stage end
  end_to_end_dur DURATION
) AS
WITH
  -- Story time windows and renderer pid per page.
  story_bounds AS (
    SELECT
      s.page,
      sp.pid,
      s.story_start,
      s.visual_mark,
      s.interactive_mark,
      s.story_finish
    FROM chrome_loadline2_stages s
    JOIN _chrome_loadline2_story_pid sp USING (page)
  ),
  -- Identifies ThrottlingURLLoader::OnReceiveResponse slices that invoke
  -- BackgroundProcessor::MaybeStartProcessingResponse for script streaming.
  maybe_start_resp_raw AS (
    SELECT
      sb.page,
      anc.id AS resp_slice_id,
      anc.ts AS response_start_ts,
      MIN(EXTRACT_ARG(m.arg_set_id, 'debug.url')) AS script_url,
      STR_SPLIT(
        MIN(EXTRACT_ARG(m.arg_set_id, 'debug.url')), '#', 0
      ) AS norm_url
    FROM story_bounds sb
    JOIN thread_slice m ON m.pid = sb.pid
      AND m.ts >= sb.story_start
      AND m.ts < sb.story_finish
      AND m.name = 'BackgroundProcessor::MaybeStartProcessingResponse'
    JOIN ancestor_slice(m.id) anc
      ON anc.name = 'ThrottlingURLLoader::OnReceiveResponse'
    GROUP BY sb.page, anc.id, anc.ts
  ),
  maybe_start_resp AS (
    SELECT
      page,
      resp_slice_id,
      response_start_ts,
      script_url,
      norm_url,
      ROW_NUMBER() OVER (
        PARTITION BY page, norm_url
        ORDER BY response_start_ts, resp_slice_id
      ) AS url_occurrence,
      COUNT(*) OVER (PARTITION BY page, norm_url) AS msr_count
    FROM maybe_start_resp_raw
  ),
  -- Extracts v8.parseOnBackground slices, their CPU/wait child durations, and
  -- the corresponding OnFinishStreaming completion timestamp.
  bg_parses_raw AS (
    SELECT
      sb.page,
      sb.pid,
      sb.story_finish,
      bg.id AS bg_slice_id,
      bg.thread_name,
      EXTRACT_ARG(bg.arg_set_id, 'debug.data.url') AS raw_url,
      STR_SPLIT(
        EXTRACT_ARG(bg.arg_set_id, 'debug.data.url'), '#', 0
      ) AS norm_url,
      bg.ts AS bg_parse_start_ts,
      bg.ts + bg.dur AS bg_parse_end_ts,
      bg.dur AS bg_parse_dur,
      ROW_NUMBER() OVER (
        PARTITION BY
          sb.page,
          STR_SPLIT(EXTRACT_ARG(bg.arg_set_id, 'debug.data.url'), '#', 0)
        ORDER BY bg.ts, bg.id
      ) AS url_occurrence,
      COUNT(*) OVER (
        PARTITION BY
          sb.page,
          STR_SPLIT(EXTRACT_ARG(bg.arg_set_id, 'debug.data.url'), '#', 0)
      ) AS bg_count
    FROM story_bounds sb
    JOIN thread_slice bg ON bg.pid = sb.pid
      AND bg.ts >= sb.story_start
      AND bg.ts < sb.story_finish
      AND bg.name = 'v8.parseOnBackground'
    WHERE EXTRACT_ARG(bg.arg_set_id, 'debug.data.url') IS NOT NULL
  ),
  bg_parses AS (
    SELECT
      bp.page,
      bp.bg_slice_id,
      bp.thread_name,
      bp.raw_url,
      bp.norm_url,
      bp.bg_parse_start_ts,
      bp.bg_parse_end_ts,
      bp.bg_parse_dur,
      COALESCE((
        SELECT SUM(c.dur)
        FROM descendant_slice(bp.bg_slice_id) c
        WHERE c.name = 'v8.parseOnBackgroundParsing'
      ), 0) AS bg_parse_cpu_dur,
      COALESCE((
        SELECT SUM(c.dur)
        FROM descendant_slice(bp.bg_slice_id) c
        WHERE c.name = 'v8.parseOnBackgroundWaiting'
      ), 0) AS bg_parse_wait_dur,
      COALESCE(
        msr_occ.resp_slice_id,
        (
          SELECT msr.resp_slice_id
          FROM maybe_start_resp msr
          WHERE msr.page = bp.page
            AND msr.norm_url = bp.norm_url
            AND msr.response_start_ts <= bp.bg_parse_start_ts
          ORDER BY msr.response_start_ts DESC, msr.resp_slice_id DESC
          LIMIT 1
        )
      ) AS resp_slice_id,
      (
        SELECT MIN(f.ts + f.dur)
        FROM thread_slice f
        WHERE f.pid = bp.pid
          AND f.name IN (
            'BackgroundProcessor::OnFinishStreaming',
            'BackgroundJSStreamManager::OnFinishStreaming'
          )
          AND f.ts >= bp.bg_parse_end_ts
          AND f.ts < bp.story_finish
          AND STR_SPLIT(EXTRACT_ARG(f.arg_set_id, 'debug.url'), '#', 0) =
            bp.norm_url
      ) AS finish_streaming_end_ts,
      bp.url_occurrence
    FROM bg_parses_raw bp
    LEFT JOIN maybe_start_resp msr_occ
      ON msr_occ.page = bp.page
      AND msr_occ.norm_url = bp.norm_url
      AND msr_occ.msr_count = bp.bg_count
      AND msr_occ.url_occurrence = bp.url_occurrence
      AND msr_occ.response_start_ts <= bp.bg_parse_start_ts
  ),
  -- Collects renderer network response slices, excluding ServiceWorker
  -- passthrough responses that do not attach a BackgroundProcessor, and marks
  -- responses that actually spawned a v8.parseOnBackground task.
  net_responses_raw AS (
    SELECT
      sb.page,
      r.id AS resp_slice_id,
      CASE
        WHEN EXISTS (
          SELECT 1
          FROM bg_parses bg
          WHERE bg.page = sb.page AND bg.resp_slice_id = r.id
        ) THEN 1
        ELSE 0
      END AS is_bg_parsed,
      COALESCE(
        msr.script_url, EXTRACT_ARG(r.arg_set_id, 'debug.url')
      ) AS raw_url,
      STR_SPLIT(
        COALESCE(msr.script_url, EXTRACT_ARG(r.arg_set_id, 'debug.url')),
        '#',
        0
      ) AS norm_url,
      r.ts AS response_start_ts,
      r.ts + r.dur AS response_slice_end_ts
    FROM story_bounds sb
    JOIN thread_slice r ON r.pid = sb.pid
      AND r.ts >= sb.story_start
      AND r.ts < sb.story_finish
      AND r.name = 'ThrottlingURLLoader::OnReceiveResponse'
    LEFT JOIN maybe_start_resp msr
      ON msr.page = sb.page AND msr.resp_slice_id = r.id
    WHERE
      COALESCE(
        msr.script_url, EXTRACT_ARG(r.arg_set_id, 'debug.url')
      ) IS NOT NULL
      AND (
        msr.resp_slice_id IS NOT NULL
        OR COALESCE(r.thread_name, '') != 'ServiceWorker thread'
      )
  ),
  -- Follows Mojo flow events from OnReceiveResponse back to
  -- ThrottlingURLLoader::Start/StartNow and forward to ~ThrottlingURLLoader.
  net_with_flows AS (
    SELECT
      nr.page,
      nr.resp_slice_id,
      nr.is_bg_parsed,
      nr.raw_url,
      nr.norm_url,
      MIN(COALESCE(
        s_start0.ts, s_start.ts, s_now0.ts, s_now.ts
      )) AS request_start_ts,
      nr.response_start_ts,
      MAX(
        COALESCE(d.ts + d.dur, nr.response_slice_end_ts)
      ) AS response_end_ts,
      ROW_NUMBER() OVER (
        PARTITION BY nr.page, nr.is_bg_parsed, nr.norm_url
        ORDER BY nr.response_start_ts, nr.resp_slice_id
      ) AS url_occurrence
    FROM net_responses_raw nr
    LEFT JOIN flow f1 ON f1.slice_in = nr.resp_slice_id
    LEFT JOIN slice s_now
      ON s_now.id = f1.slice_out
      AND s_now.name = 'ThrottlingURLLoader::StartNow'
    LEFT JOIN flow f2 ON f2.slice_in = s_now.id
    LEFT JOIN slice s_start
      ON s_start.id = f2.slice_out
      AND s_start.name = 'ThrottlingURLLoader::Start'
    LEFT JOIN slice s_now0
      ON s_now0.id = f2.slice_out
      AND s_now0.name = 'ThrottlingURLLoader::StartNow'
    LEFT JOIN flow f0 ON f0.slice_in = s_now0.id
    LEFT JOIN slice s_start0
      ON s_start0.id = f0.slice_out
      AND s_start0.name = 'ThrottlingURLLoader::Start'
    LEFT JOIN flow f3 ON f3.slice_out = nr.resp_slice_id
    LEFT JOIN slice d
      ON d.id = f3.slice_in
      AND d.name = 'ThrottlingURLLoader::~ThrottlingURLLoader'
    GROUP BY
      nr.page,
      nr.resp_slice_id,
      nr.is_bg_parsed,
      nr.raw_url,
      nr.norm_url,
      nr.response_start_ts,
      nr.response_slice_end_ts
  ),
  -- Pairs adjacent v8.compile / v8.run and v8.compileModule / v8.evaluateModule
  -- slices at the same track and depth.
  v8_pairs_raw AS (
    SELECT
      sb.page,
      ts.id AS slice_id,
      ts.name,
      ts.ts,
      ts.dur,
      ts.thread_name,
      ts.track_id,
      ts.depth,
      ts.arg_set_id,
      LEAD(ts.id) OVER (
        PARTITION BY
          sb.page,
          ts.track_id,
          ts.depth,
          ts.name IN ('v8.compileModule', 'v8.evaluateModule')
        ORDER BY ts.ts
      ) AS next_id,
      LEAD(ts.name) OVER (
        PARTITION BY
          sb.page,
          ts.track_id,
          ts.depth,
          ts.name IN ('v8.compileModule', 'v8.evaluateModule')
        ORDER BY ts.ts
      ) AS next_name,
      LEAD(ts.ts) OVER (
        PARTITION BY
          sb.page,
          ts.track_id,
          ts.depth,
          ts.name IN ('v8.compileModule', 'v8.evaluateModule')
        ORDER BY ts.ts
      ) AS next_ts,
      LEAD(ts.dur) OVER (
        PARTITION BY
          sb.page,
          ts.track_id,
          ts.depth,
          ts.name IN ('v8.compileModule', 'v8.evaluateModule')
        ORDER BY ts.ts
      ) AS next_dur,
      LEAD(ts.arg_set_id) OVER (
        PARTITION BY
          sb.page,
          ts.track_id,
          ts.depth,
          ts.name IN ('v8.compileModule', 'v8.evaluateModule')
        ORDER BY ts.ts
      ) AS next_arg_set_id
    FROM story_bounds sb
    JOIN thread_slice ts ON ts.pid = sb.pid
      AND ts.ts >= sb.story_start
      AND ts.ts < sb.story_finish
      AND ts.name IN (
        'v8.compile', 'v8.run', 'v8.compileModule', 'v8.evaluateModule'
      )
  ),
  -- Extracts compile + execute pairs and classifies streaming / inline status.
  compiled_raw AS (
    SELECT
      page,
      slice_id AS compile_slice_id,
      name AS compile_slice_name,
      ts AS compile_ts,
      dur AS compile_dur,
      CASE
        WHEN (name = 'v8.compile' AND next_name = 'v8.run')
          OR (name = 'v8.compileModule' AND next_name = 'v8.evaluateModule')
        THEN next_id
        ELSE NULL
      END AS execute_slice_id,
      CASE
        WHEN (name = 'v8.compile' AND next_name = 'v8.run')
          OR (name = 'v8.compileModule' AND next_name = 'v8.evaluateModule')
        THEN next_ts
        ELSE NULL
      END AS execute_ts,
      CASE
        WHEN (name = 'v8.compile' AND next_name = 'v8.run')
          OR (name = 'v8.compileModule' AND next_name = 'v8.evaluateModule')
        THEN next_dur
        ELSE NULL
      END AS execute_dur,
      thread_name,
      NULLIF(
        COALESCE(
          EXTRACT_ARG(arg_set_id, 'debug.fileName'),
          EXTRACT_ARG(arg_set_id, 'debug.data.url'),
          EXTRACT_ARG(next_arg_set_id, 'debug.fileName')
        ),
        ''
      ) AS raw_url,
      EXTRACT_ARG(arg_set_id, 'debug.data.lineNumber') AS line_number,
      EXTRACT_ARG(arg_set_id, 'debug.data.columnNumber') AS column_number,
      CASE
        WHEN (
            COALESCE(
              EXTRACT_ARG(arg_set_id, 'debug.data.notStreamedReason'), ''
            ) = 'inline script'
            AND COALESCE(thread_name, '') = 'CrRendererMain'
          )
          OR NULLIF(
            COALESCE(
              EXTRACT_ARG(arg_set_id, 'debug.fileName'),
              EXTRACT_ARG(arg_set_id, 'debug.data.url'),
              EXTRACT_ARG(next_arg_set_id, 'debug.fileName')
            ),
            ''
          ) IS NULL
        THEN 1
        ELSE 0
      END AS is_inline,
      CASE
        WHEN EXTRACT_ARG(arg_set_id, 'debug.data.streamed') = 1 THEN 1
        ELSE 0
      END AS is_streamed,
      CASE
        WHEN COALESCE(EXTRACT_ARG(arg_set_id, 'debug.data.streamed'), 0) = 1
          THEN NULL
        WHEN COALESCE(
            EXTRACT_ARG(arg_set_id, 'debug.data.consumedCacheSize'), 0
          ) > 0
          THEN 'code cache hit'
        WHEN COALESCE(
            EXTRACT_ARG(arg_set_id, 'debug.data.notStreamedReason'), ''
          ) = 'inline script'
          AND COALESCE(thread_name, '') != 'CrRendererMain'
          THEN 'worker importScripts'
        ELSE COALESCE(
          EXTRACT_ARG(arg_set_id, 'debug.data.notStreamedReason'),
          CASE WHEN name = 'v8.compileModule' THEN 'module script' ELSE NULL END
        )
      END AS not_streamed_reason
    FROM v8_pairs_raw
    WHERE name IN ('v8.compile', 'v8.compileModule')
  ),
  -- Numbers occurrences of each normalized external URL per page and streaming
  -- mode in compile order so repeated loads match their corresponding
  -- streamed (bg_parses) or non-streamed (net_with_flows) network rows.
  compiled_numbered AS (
    SELECT
      *,
      STR_SPLIT(raw_url, '#', 0) AS norm_url,
      CASE
        WHEN is_inline = 0 THEN ROW_NUMBER() OVER (
          PARTITION BY page, is_inline, is_streamed, STR_SPLIT(raw_url, '#', 0)
          ORDER BY compile_ts, compile_slice_id
        )
        ELSE NULL
      END AS url_occurrence
    FROM compiled_raw
  ),
  -- Joins compiled scripts with background parses and network flows, and unions
  -- preloaded background-streamed scripts that never compiled or executed.
  all_scripts_raw AS (
    SELECT
      c.page,
      COALESCE(
        c.raw_url, bg.raw_url, net_bg.raw_url, net_non_bg.raw_url
      ) AS raw_url,
      c.line_number,
      c.column_number,
      c.is_inline,
      c.is_streamed,
      c.not_streamed_reason,
      CASE
        WHEN c.is_inline = 1 THEN 'inline'
        WHEN c.compile_slice_name = 'v8.compileModule' THEN 'module'
        WHEN c.is_streamed = 1 OR bg.bg_slice_id IS NOT NULL THEN 'streamed'
        ELSE 'external_non_streamed'
      END AS script_kind,
      c.thread_name,
      COALESCE(
        net_bg.request_start_ts, net_non_bg.request_start_ts
      ) AS request_start_ts,
      COALESCE(
        net_bg.response_start_ts, net_non_bg.response_start_ts
      ) AS response_start_ts,
      COALESCE(
        bg.finish_streaming_end_ts,
        net_bg.response_end_ts,
        net_non_bg.response_end_ts
      ) AS response_end_ts,
      bg.bg_parse_start_ts,
      bg.bg_parse_end_ts,
      bg.bg_parse_dur,
      bg.bg_parse_cpu_dur,
      bg.bg_parse_wait_dur,
      c.compile_ts,
      c.compile_dur,
      c.execute_ts,
      c.execute_dur
    FROM compiled_numbered c
    LEFT JOIN bg_parses bg
      ON c.is_inline = 0
      AND c.is_streamed = 1
      AND bg.page = c.page
      AND bg.norm_url = c.norm_url
      AND bg.url_occurrence = c.url_occurrence
    LEFT JOIN net_with_flows net_bg
      ON bg.resp_slice_id IS NOT NULL
      AND net_bg.page = bg.page
      AND net_bg.resp_slice_id = bg.resp_slice_id
    LEFT JOIN net_with_flows net_non_bg
      ON c.is_inline = 0
      AND c.is_streamed = 0
      AND bg.bg_slice_id IS NULL
      AND net_non_bg.page = c.page
      AND net_non_bg.is_bg_parsed = 0
      AND net_non_bg.norm_url = c.norm_url
      AND net_non_bg.url_occurrence = c.url_occurrence
    UNION ALL
    SELECT
      bg.page,
      COALESCE(bg.raw_url, net_bg.raw_url) AS raw_url,
      NULL AS line_number,
      NULL AS column_number,
      0 AS is_inline,
      1 AS is_streamed,
      'preloaded (never executed)' AS not_streamed_reason,
      'preloaded_unused' AS script_kind,
      bg.thread_name,
      net_bg.request_start_ts,
      net_bg.response_start_ts,
      COALESCE(
        bg.finish_streaming_end_ts, net_bg.response_end_ts
      ) AS response_end_ts,
      bg.bg_parse_start_ts,
      bg.bg_parse_end_ts,
      bg.bg_parse_dur,
      bg.bg_parse_cpu_dur,
      bg.bg_parse_wait_dur,
      NULL AS compile_ts,
      NULL AS compile_dur,
      NULL AS execute_ts,
      NULL AS execute_dur
    FROM bg_parses bg
    LEFT JOIN net_with_flows net_bg
      ON net_bg.page = bg.page
      AND net_bg.resp_slice_id = bg.resp_slice_id
    WHERE NOT EXISTS (
      SELECT 1
      FROM compiled_numbered c
      WHERE c.is_inline = 0
        AND c.is_streamed = 1
        AND c.page = bg.page
        AND c.norm_url = bg.norm_url
        AND c.url_occurrence = bg.url_occurrence
    )
  )
SELECT
  ROW_NUMBER() OVER (
    ORDER BY
      a.page,
      COALESCE(
        a.request_start_ts,
        a.response_start_ts,
        a.bg_parse_start_ts,
        a.compile_ts
      ),
      a.compile_ts,
      a.bg_parse_start_ts,
      a.execute_ts,
      a.raw_url,
      a.line_number,
      a.column_number
  ) AS script_id,
  a.page,
  -- Extracts a short label: inline line:col, data/blob URI prefix, Amazon
  -- AUIClients/ bundle name (amazon_product), Wikipedia ResourceLoader
  -- modules= list (wikipedia_article), or the URL path basename.
  CASE
    WHEN a.is_inline = 1 THEN
      'inline (line ' || CAST(COALESCE(a.line_number, 1) AS TEXT) || ':' ||
      CAST(COALESCE(a.column_number, 1) AS TEXT) || ')'
    WHEN a.raw_url GLOB 'data:*' THEN 'data:text/javascript'
    WHEN a.raw_url GLOB 'blob:*' THEN
      'blob:' || SUBSTR(
        COALESCE(REGEXP_EXTRACT(a.raw_url, '([^/]+)$'), 'script'), 1, 36
      )
    ELSE SUBSTR(
      COALESCE(
        REGEXP_EXTRACT(a.raw_url, 'AUIClients/([^#&]+)'),
        REGEXP_EXTRACT(a.raw_url, 'modules=([^#&]+)'),
        REGEXP_EXTRACT(a.raw_url, '([^/?#]+)(?:[?#].*)?$'),
        a.raw_url,
        'script'
      ),
      1,
      64
    )
  END AS script_name,
  CASE
    WHEN a.raw_url GLOB 'data:*' THEN SUBSTR(a.raw_url, 1, 120)
    ELSE NULLIF(a.raw_url, '')
  END AS url,
  a.script_kind,
  a.is_inline,
  a.is_streamed,
  a.not_streamed_reason,
  a.thread_name,
  CASE
    WHEN a.compile_ts IS NULL AND a.execute_ts IS NULL THEN 'never_executed'
    WHEN sb.visual_mark IS NULL OR sb.interactive_mark IS NULL THEN NULL
    WHEN COALESCE(
        a.execute_ts + a.execute_dur, a.compile_ts + a.compile_dur
      ) <= sb.visual_mark
      THEN 'before_visual'
    WHEN COALESCE(a.compile_ts, a.execute_ts) < sb.visual_mark
      AND COALESCE(
        a.execute_ts + a.execute_dur, a.compile_ts + a.compile_dur
      ) > sb.visual_mark
      THEN 'spanning_visual'
    WHEN COALESCE(
        a.execute_ts + a.execute_dur, a.compile_ts + a.compile_dur
      ) <= sb.interactive_mark
      THEN 'before_interactive'
    WHEN COALESCE(a.compile_ts, a.execute_ts) < sb.interactive_mark
      AND COALESCE(
        a.execute_ts + a.execute_dur, a.compile_ts + a.compile_dur
      ) > sb.interactive_mark
      THEN 'spanning_interactive'
    ELSE 'after_interactive'
  END AS milestone_status,
  a.request_start_ts,
  a.response_start_ts,
  a.response_end_ts,
  a.response_start_ts - a.request_start_ts AS request_dur,
  a.response_end_ts - a.response_start_ts AS response_dur,
  a.bg_parse_start_ts,
  a.bg_parse_end_ts,
  a.bg_parse_dur,
  a.bg_parse_cpu_dur,
  a.bg_parse_wait_dur,
  a.compile_ts,
  a.compile_dur,
  a.execute_ts,
  a.execute_dur,
  MAX(
    COALESCE(a.execute_ts + a.execute_dur, 0),
    COALESCE(a.compile_ts + a.compile_dur, 0),
    COALESCE(a.bg_parse_end_ts, 0),
    COALESCE(a.response_end_ts, 0)
  ) - COALESCE(
    a.request_start_ts,
    a.response_start_ts,
    a.bg_parse_start_ts,
    a.compile_ts,
    a.execute_ts
  ) AS end_to_end_dur
FROM all_scripts_raw a
JOIN story_bounds sb USING (page)
ORDER BY script_id;

-- Individual timeline slices for each stage of a JS script lifecycle.
CREATE PERFETTO TABLE chrome_loadline2_js_script_slices (
  -- Unique slice row identifier
  id LONG,
  -- Foreign key into chrome_loadline2_js_scripts
  script_id LONG,
  -- Name of the page
  page STRING,
  -- Lifecycle stage ('request', 'response', 'bg_parse', 'compile', 'execute')
  stage STRING,
  -- Stage start timestamp
  ts TIMESTAMP,
  -- Stage duration
  dur DURATION,
  -- Heuristic human-readable script or bundle name derived from the URL
  script_name STRING,
  -- Script URL (or document URL for inline scripts when available, else NULL)
  url STRING,
  -- Script classification ('streamed', 'external_non_streamed', 'module',
  -- 'preloaded_unused', 'inline')
  script_kind STRING,
  -- True if inline script, false if external script
  is_inline BOOL,
  -- True if parsed on background streaming thread, false otherwise
  is_streamed BOOL,
  -- Reason why script was not streamed (or 'preloaded (never executed)' for
  -- unused preloads), or NULL if background-streamed and executed
  not_streamed_reason STRING,
  -- Thread where compilation/execution (or background parse for unused
  -- preloads) occurred
  thread_name STRING,
  -- Execution timing relative to visual_mark and interactive_mark
  -- ('before_visual', 'spanning_visual', 'before_interactive',
  -- 'spanning_interactive', 'after_interactive', 'never_executed', or NULL if
  -- marks are missing)
  milestone_status STRING
) AS
WITH
  raw_slices AS (
    SELECT
      script_id,
      page,
      'request' AS stage,
      request_start_ts AS ts,
      request_dur AS dur,
      script_name,
      url,
      script_kind,
      is_inline,
      is_streamed,
      not_streamed_reason,
      thread_name,
      milestone_status
    FROM chrome_loadline2_js_scripts
    WHERE request_start_ts IS NOT NULL AND request_dur > 0
    UNION ALL
    SELECT
      script_id,
      page,
      'response' AS stage,
      response_start_ts AS ts,
      response_dur AS dur,
      script_name,
      url,
      script_kind,
      is_inline,
      is_streamed,
      not_streamed_reason,
      thread_name,
      milestone_status
    FROM chrome_loadline2_js_scripts
    WHERE response_start_ts IS NOT NULL AND response_dur > 0
    UNION ALL
    SELECT
      script_id,
      page,
      'bg_parse' AS stage,
      bg_parse_start_ts AS ts,
      bg_parse_dur AS dur,
      script_name,
      url,
      script_kind,
      is_inline,
      is_streamed,
      not_streamed_reason,
      thread_name,
      milestone_status
    FROM chrome_loadline2_js_scripts
    WHERE bg_parse_start_ts IS NOT NULL AND bg_parse_dur > 0
    UNION ALL
    SELECT
      script_id,
      page,
      'compile' AS stage,
      compile_ts AS ts,
      compile_dur AS dur,
      script_name,
      url,
      script_kind,
      is_inline,
      is_streamed,
      not_streamed_reason,
      thread_name,
      milestone_status
    FROM chrome_loadline2_js_scripts
    WHERE compile_ts IS NOT NULL AND compile_dur > 0
    UNION ALL
    SELECT
      script_id,
      page,
      'execute' AS stage,
      execute_ts AS ts,
      execute_dur AS dur,
      script_name,
      url,
      script_kind,
      is_inline,
      is_streamed,
      not_streamed_reason,
      thread_name,
      milestone_status
    FROM chrome_loadline2_js_scripts
    WHERE execute_ts IS NOT NULL AND execute_dur > 0
  )
SELECT
  ROW_NUMBER() OVER (ORDER BY page, ts, script_id, stage) AS id,
  script_id,
  page,
  stage,
  ts,
  dur,
  script_name,
  url,
  script_kind,
  is_inline,
  is_streamed,
  not_streamed_reason,
  thread_name,
  milestone_status
FROM raw_slices
ORDER BY id;

