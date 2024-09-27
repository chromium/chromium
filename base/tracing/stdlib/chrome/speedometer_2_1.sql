-- Copyright 2024 The Chromium Authors
-- Use of this source code is governed by a BSD-style license that can be
-- found in the LICENSE file.

-- List Speedometer 2.1 test marks. Used to find relevant slices.
CREATE PERFETTO VIEW _chrome_speedometer_2_1_mark_name(
  -- Expected slice name
  name STRING,
  -- Suite name
  suite_name STRING,
  -- Test name
  test_name STRING,
  -- Mark type
  mark_type STRING)
AS
WITH
  data(suite_name, test_name)
  AS (
    VALUES('Angular2-TypeScript-TodoMVC', 'Adding100Items'),
    ('Angular2-TypeScript-TodoMVC', 'CompletingAllItems'),
    ('Angular2-TypeScript-TodoMVC', 'DeletingItems'),
    ('AngularJS-TodoMVC', 'Adding100Items'),
    ('AngularJS-TodoMVC', 'CompletingAllItems'),
    ('AngularJS-TodoMVC', 'DeletingAllItems'),
    ('BackboneJS-TodoMVC', 'Adding100Items'),
    ('BackboneJS-TodoMVC', 'CompletingAllItems'),
    ('BackboneJS-TodoMVC', 'DeletingAllItems'),
    ('Elm-TodoMVC', 'Adding100Items'),
    ('Elm-TodoMVC', 'CompletingAllItems'),
    ('Elm-TodoMVC', 'DeletingItems'),
    ('EmberJS-Debug-TodoMVC', 'Adding100Items'),
    ('EmberJS-Debug-TodoMVC', 'CompletingAllItems'),
    ('EmberJS-Debug-TodoMVC', 'DeletingItems'),
    ('EmberJS-TodoMVC', 'Adding100Items'),
    ('EmberJS-TodoMVC', 'CompletingAllItems'),
    ('EmberJS-TodoMVC', 'DeletingItems'),
    ('Flight-TodoMVC', 'Adding100Items'),
    ('Flight-TodoMVC', 'CompletingAllItems'),
    ('Flight-TodoMVC', 'DeletingItems'),
    ('Inferno-TodoMVC', 'Adding100Items'),
    ('Inferno-TodoMVC', 'CompletingAllItems'),
    ('Inferno-TodoMVC', 'DeletingItems'),
    ('Preact-TodoMVC', 'Adding100Items'),
    ('Preact-TodoMVC', 'CompletingAllItems'),
    ('Preact-TodoMVC', 'DeletingItems'),
    ('React-Redux-TodoMVC', 'Adding100Items'),
    ('React-Redux-TodoMVC', 'CompletingAllItems'),
    ('React-Redux-TodoMVC', 'DeletingItems'),
    ('React-TodoMVC', 'Adding100Items'),
    ('React-TodoMVC', 'CompletingAllItems'),
    ('React-TodoMVC', 'DeletingAllItems'),
    ('Vanilla-ES2015-Babel-Webpack-TodoMVC', 'Adding100Items'),
    ('Vanilla-ES2015-Babel-Webpack-TodoMVC', 'CompletingAllItems'),
    ('Vanilla-ES2015-Babel-Webpack-TodoMVC', 'DeletingItems'),
    ('Vanilla-ES2015-TodoMVC', 'Adding100Items'),
    ('Vanilla-ES2015-TodoMVC', 'CompletingAllItems'),
    ('Vanilla-ES2015-TodoMVC', 'DeletingItems'),
    ('VanillaJS-TodoMVC', 'Adding100Items'),
    ('VanillaJS-TodoMVC', 'CompletingAllItems'),
    ('VanillaJS-TodoMVC', 'DeletingAllItems'),
    ('VueJS-TodoMVC', 'Adding100Items'),
    ('VueJS-TodoMVC', 'CompletingAllItems'),
    ('VueJS-TodoMVC', 'DeletingAllItems'),
    ('jQuery-TodoMVC', 'Adding100Items'),
    ('jQuery-TodoMVC', 'CompletingAllItems'),
    ('jQuery-TodoMVC', 'DeletingAllItems')
  ),
  mark_type(mark_type) AS (VALUES('start'), ('sync-end'), ('async-end'))
SELECT
  suite_name || '.' || test_name || '-' || mark_type AS name,
  suite_name,
  test_name,
  mark_type
FROM data, mark_type;

-- Augmented slices for Speedometer measurements.
-- These are the intervals of time Speedometer uses to compute the final score.
-- There are two intervals that are measured for every test: sync and async
-- sync is the time between the start and sync-end marks, async is the time
-- between the sync-end and async-end marks.
CREATE PERFETTO TABLE chrome_speedometer_2_1_measure(
  -- Start timestamp of the measure slice
  ts INT,
  -- Duration of the measure slice
  dur INT,
  -- Full measure name
  name STRING,
  -- Speedometer iteration the slice belongs to.
  iteration INT,
  -- Suite name
  suite_name STRING,
  -- Test name
  test_name STRING,
  -- Type of the measure (sync or async)
  measure_type STRING)
AS
WITH
  mark AS (
    SELECT
      s.id AS slice_id,
      RANK() OVER (PARTITION BY name ORDER BY ts ASC) AS iteration,
      m.suite_name,
      m.test_name,
      m.mark_type
    FROM slice AS s
    -- Make sure we only look at slices with names we expect.
    JOIN _chrome_speedometer_2_1_mark_name AS m
      USING (name)
    WHERE category = 'blink.user_timing'
  ),
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
    FROM mark
    JOIN slice
      USING (slice_id)
  ),
  filtered AS (
    SELECT *
    FROM augmented
    -- This server 2 purposes: make sure we have all the marks (think truncated
    -- trace), and remove the NULL ts values due to the LAG window function.
    WHERE mark_count = 3
  ),
  base AS (
    SELECT
      sync_end_ts AS ts,
      async_end_ts - sync_end_ts AS dur,
      iteration,
      suite_name,
      test_name,
      'async' AS measure_type
    FROM filtered
    UNION ALL
    SELECT
      start_ts AS ts,
      sync_end_ts - start_ts AS dur,
      iteration,
      suite_name,
      test_name,
      'sync' AS measure_type
    FROM filtered
  )
SELECT
  ts,
  dur,
  suite_name || '.' || test_name || '-' || measure_type AS name,
  iteration,
  suite_name,
  test_name,
  measure_type
FROM base;

-- Slice that covers one Speedometer iteration.
-- This slice is actually estimated as a default Speedometer run will not emit
-- marks to cover this interval. The metrics associated are the same ones
-- Speedometer would output, but note we use ns precision (Speedometer uses
-- ~100us) so the actual values might differ a bit. Also note Speedometer
-- returns the values in ms these here and in ns.
CREATE PERFETTO TABLE chrome_speedometer_2_1_iteration(
  -- Start timestamp of the iteration
  ts INT,
  -- Duration of the iteration
  dur INT,
  -- Iteration name
  name STRING,
  -- Iteration number
  iteration INT,
  -- Geometric mean of the suite durations for this iteration.
  geomean DOUBLE,
  -- Speedometer score for this iteration (The total score for a run in the
  -- average of all iteration scores).
  score DOUBLE)
AS
SELECT
  MIN(start) AS ts,
  MAX(END) - MIN(start) AS dur,
  'iteration-' || iteration AS name,
  iteration,
  -- Compute geometric mean using LN instead of multiplication to prevent
  -- overflows
  EXP(AVG(LN(suite_total))) AS geomean,
  1000 / EXP(AVG(LN(suite_total))) * 60 / 3 AS score
FROM
  (
    SELECT
      iteration,
      SUM(dur / (1000.0 * 1000.0)) AS suite_total,
      MIN(ts) AS start,
      MAX(ts + dur) AS END
    FROM chrome_speedometer_2_1_measure
    GROUP BY suite_name, iteration
  )
GROUP BY iteration;

-- Returns the Speedometer 2.1 score for all iterations in the trace
CREATE PERFETTO FUNCTION chrome_speedometer_2_1_score()
-- Speedometer 2.1 score
RETURNS DOUBLE
AS
SELECT AVG(score) FROM chrome_speedometer_2_1_iteration;

-- Returns the utid for the main thread that ran Speedometer 2.1
CREATE PERFETTO FUNCTION chrome_speedometer_2_1_renderer_main_utid()
-- Renderer main utid
RETURNS INT
AS
SELECT utid
FROM thread_track
WHERE
  id IN (
    SELECT track_id
    FROM slice, _chrome_speedometer_2_1_mark_name
    USING (name)
    WHERE category = 'blink.user_timing'
  );
