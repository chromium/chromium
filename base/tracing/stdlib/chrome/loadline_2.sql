-- Copyright 2025 The Chromium Authors
-- Use of this source code is governed by a BSD-style license that can be
-- found in the LICENSE file.

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
