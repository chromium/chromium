-- Copyright 2023 The Chromium Authors
-- Use of this source code is governed by a BSD-style license that can be
-- found in the LICENSE file.

INCLUDE PERFETTO MODULE chrome.speedometer_2_1;
INCLUDE PERFETTO MODULE chrome.speedometer_3;

CREATE PERFETTO FUNCTION _chrome_speedometer_version()
RETURNS STRING
AS
WITH
  num_measures AS (
    SELECT '2.1' AS version, COUNT(*) AS num_measures
    FROM chrome_speedometer_2_1_measure
    UNION ALL
    SELECT '3' AS version, COUNT(*) AS num_measures
    FROM chrome_speedometer_3_measure
  )
SELECT version
FROM num_measures
ORDER BY num_measures DESC
LIMIT 1;

-- Augmented slices for Speedometer measurements.
-- These are the intervals of time Speedometer uses to compute the final score.
-- There are two intervals that are measured for every test: sync and async
CREATE PERFETTO TABLE chrome_speedometer_measure(
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
  all_versions AS (
    SELECT '2.1' AS version, * FROM chrome_speedometer_2_1_measure
    UNION ALL
    SELECT '3' AS version, * FROM chrome_speedometer_3_measure
  )
SELECT ts, dur, name, iteration, suite_name, test_name, measure_type
FROM all_versions
WHERE version = _chrome_speedometer_version();

-- Slice that covers one Speedometer iteration.
-- Depending on the Speedometer version these slices might need to be estimated
-- as older versions of Speedometer to not emit marks for this interval. The
-- metrics associated are the same ones Speedometer would output, but note we
-- use ns precision (Speedometer uses ~100us) so the actual values might differ
-- a bit.
CREATE PERFETTO TABLE chrome_speedometer_iteration(
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
WITH
  all_versions AS (
    SELECT '2.1' AS version, * FROM chrome_speedometer_2_1_iteration
    UNION ALL
    SELECT '3' AS version, * FROM chrome_speedometer_3_iteration
  )
SELECT ts, dur, name, iteration, geomean, score
FROM all_versions
WHERE version = _chrome_speedometer_version();

-- Returns the Speedometer score for all iterations in the trace
CREATE PERFETTO FUNCTION chrome_speedometer_score()
-- Speedometer score
RETURNS DOUBLE
AS
SELECT
  IIF(
    _chrome_speedometer_version() = '3',
    chrome_speedometer_3_score(),
    chrome_speedometer_2_1_score());

-- Returns the utid for the main thread that ran Speedometer 3
CREATE PERFETTO FUNCTION chrome_speedometer_renderer_main_utid()
-- Renderer main utid
RETURNS INT
AS
SELECT
  IIF(
    _chrome_speedometer_version() = '3',
    chrome_speedometer_3_renderer_main_utid(),
    chrome_speedometer_2_1_renderer_main_utid());
