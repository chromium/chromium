-- Copyright 2019 The Chromium Authors
-- Use of this source code is governed by a BSD-style license that can be
-- found in the LICENSE file.

-- Annotates a trace with Speedometer 2.1 related information.
--
-- The scripts below analyse traces with the following tracing options
-- enabled:
--
--  - Chromium:
--      "blink.user_timing".
--
--  NOTE: A regular speedometer run (e.g. from the website) will generate the
--  required events. No need to add any extra JS or anything.
--
-- Noteworthy tables:
--   speedometer_mark: List of marks (event slices) emitted by Speedometer.
--       These are the points in time Speedometer makes a clock reading to
--       compute intervals of time for the final score.
--   speedometer_measure_slice: Augmented slices for Speedometer measurements.
--       These are the intervals of time Speedometer uses to compute the final
--       score.
--   speedometer_iteration_slice: Slice that covers one Speedometer iteration
--       and has the total_time and score for it. If you average all the scores
--       over all iterations you get the final Speedometer score for the run.

-- List of marks (event slices) emitted by Speedometer.
-- These are the points in time Speedometer makes a clock reading to compute
-- intervals of time for the final score.
--
-- @column slice_id      Slice this data refers to.
-- @column iteration     Speedometer iteration the mark belongs to.
-- @column suite_name    Suite name
-- @column test_name     Test name
-- @column mark_type     Type of mark (start, sync-end, async-end)
CREATE VIEW internal_chrome_speedometer_mark
AS
WITH
  speedometer_21_suite_name(suite_name) AS (
    VALUES
      ('VanillaJS-TodoMVC'),
      ('Vanilla-ES2015-TodoMVC'),
      ('Vanilla-ES2015-Babel-Webpack-TodoMVC'),
      ('React-TodoMVC'),
      ('React-Redux-TodoMVC'),
      ('EmberJS-TodoMVC'),
      ('EmberJS-Debug-TodoMVC'),
      ('BackboneJS-TodoMVC'),
      ('AngularJS-TodoMVC'),
      ('Angular2-TypeScript-TodoMVC'),
      ('VueJS-TodoMVC'),
      ('jQuery-TodoMVC'),
      ('Preact-TodoMVC'),
      ('Inferno-TodoMVC'),
      ('Elm-TodoMVC'),
      ('Flight-TodoMVC')
  ),
  speedometer_21_test_name(test_name) AS (
    VALUES
      ('Adding100Items'),
      ('CompletingAllItems'),
      -- This seems to be an issue with Speedometer 2.1. All tests delete all items,
      -- but for some reason the test names do not match for all suites.
      ('DeletingAllItems'),
      ('DeletingItems')
  ),
  speedometer_21_test_mark_type(mark_type) AS (
    VALUES
      ('start'),
      ('sync-end'),
      ('async-end')
  ),
  -- Make sure we only look at slices with names we expect.
  speedometer_mark_name AS (
    SELECT
      s.suite_name || '.' || t.test_name || '-' || m.mark_type AS name,
      s.suite_name,
      t.test_name,
      m.mark_type
    FROM
      speedometer_21_suite_name AS s,
      speedometer_21_test_name AS t,
      speedometer_21_test_mark_type AS m
  )
SELECT
  s.id AS slice_id,
  RANK() OVER (PARTITION BY name ORDER BY ts ASC) AS iteration,
  m.suite_name,
  m.test_name,
  m.mark_type
FROM slice AS s
JOIN speedometer_mark_name AS m
  USING (name)
WHERE category = 'blink.user_timing';

-- Augmented slices for Speedometer measurements.
-- These are the intervals of time Speedometer uses to compute the final score.
-- There are two intervals that are measured for every test: sync and async
-- sync is the time between the start and sync-end marks, async is the time
-- between the sync-end and async-end marks.
--
-- @column iteration     Speedometer iteration the mark belongs to.
-- @column suite_name    Suite name
-- @column test_name     Test name
-- @column measure_type  Type of the measure (sync or async)
-- @column ts            Start timestamp of the measure
-- @column dur           Duration of the measure
CREATE VIEW chrome_speedometer_measure
AS
WITH
  -- Get the 3 test timestamps (start, sync-end, async-end) in one row. Using a
  -- the LAG window function and partitioning by test. 2 out of the 3 rows
  -- generated per test will have some NULL ts values.
  augmented AS (
    SELECT
      iteration,
      suite_name,
      test_name,
      ts AS async_end_ts,
      LAG(ts, 1)
        OVER (PARTITION BY iteration, suite_name, test_name ORDER BY ts ASC)
        AS sync_end_ts,
      LAG(ts, 2)
        OVER (PARTITION BY iteration, suite_name, test_name ORDER BY ts ASC)
        AS start_ts,
      COUNT()
        OVER (PARTITION BY iteration, suite_name, test_name ORDER BY ts ASC)
        AS mark_count
    FROM internal_chrome_speedometer_mark
    JOIN slice
      USING (slice_id)
  ),
  filtered AS (
    SELECT *
    FROM augmented
    -- This server 2 purposes: make sure we have all the marks (think truncated
    -- trace), and remove the NULL ts values due to the LAG window function.
    WHERE mark_count = 3
  )
SELECT
  iteration,
  suite_name,
  test_name,
  'async' AS measure_type,
  sync_end_ts AS ts,
  async_end_ts - sync_end_ts AS dur
FROM filtered
UNION ALL
SELECT
  iteration,
  suite_name,
  test_name,
  'sync' AS measure_type,
  start_ts AS ts,
  sync_end_ts - start_ts AS dur
FROM filtered;

-- Slice that covers one Speedometer iteration.
-- This slice is actually estimated as a default Speedometer run will not emit
-- marks to cover this interval. The metrics associated are the same ones
-- Speedometer would output, but note we use ns precision (Speedometer uses
-- ~100us) so the actual values might differ a bit. Also note Speedometer
-- returns the values in ms these here and in ns.
--
-- @column iteration Speedometer iteration.
-- @column ts        Start timestamp of the iteration
-- @column dur       Duration of the iteration
-- @column total     Total duration of the measures in this iteration
-- @column mean      Average suite duration for this iteration.
-- @column geomean   Geometric mean of the suite durations for this iteration.
-- @column score     Speedometer score for this iteration (The total score for a
--                   run in the average of all iteration scores).
CREATE VIEW chrome_speedometer_iteration
AS
SELECT
  iteration,
  MIN(start) AS ts,
  MAX(end) - MIN(start) AS dur,
  SUM(suite_total) AS total,
  AVG(suite_total)AS mean,
  -- Compute geometric mean using LN instead of multiplication to prevent
  -- overflows
  EXP(AVG(LN(suite_total))) AS geomean,
  1e9 / EXP(AVG(LN(suite_total))) * 60 / 3 AS score
FROM
  (
    SELECT
      iteration, SUM(dur) AS suite_total, MIN(ts) AS start, MAX(ts + dur) AS end
    FROM chrome_speedometer_measure
    GROUP BY suite_name, iteration
  )
GROUP BY iteration;
