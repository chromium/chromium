-- Copyright 2026 The Chromium Authors
-- Use of this source code is governed by a BSD-style license that can be
-- found in the LICENSE file.

-- Mapping of JetStream 3 leaf benchmarks to their top-level groups and
-- worst-case counts. Each top-level group has its score calculated separately,
-- before all group scores are combined. Null top-level groups become ungrouped
-- benchmarks.
CREATE PERFETTO VIEW _chrome_jetstream_3_benchmark_name (
  name STRING,
  top_level_name STRING,
  worst_case_count LONG
) AS
WITH data(name, top_level_name, worst_case_count) AS (
  VALUES
  ('Air', NULL, 4),
  ('Basic', NULL, 4),
  ('ML', NULL, 4),
  ('Babylon', NULL, 4),
  ('cdjs', NULL, 3),
  ('first-inspector-code-load', NULL, 4),
  ('multi-inspector-code-load', NULL, 4),
  ('Box2D', NULL, 4),
  ('octane-code-load', NULL, 4),
  ('crypto', NULL, 4),
  ('delta-blue', NULL, 4),
  ('earley-boyer', NULL, 4),
  ('gbemu', NULL, 4),
  ('mandreel', NULL, 4),
  ('navier-stokes', NULL, 4),
  ('pdfjs', NULL, 4),
  ('raytrace', NULL, 4),
  ('regexp-octane', NULL, 4),
  ('richards', NULL, 4),
  ('splay', NULL, 4),
  ('typescript-octane', NULL, 2),
  ('FlightPlanner', NULL, 4),
  ('OfflineAssembler', NULL, 4),
  ('UniPoker', NULL, 4),
  ('validatorjs', NULL, 4),
  ('hash-map', NULL, 4),
  ('doxbee-promise', NULL, 4),
  ('doxbee-async', NULL, 4),
  ('ai-astar', NULL, 4),
  ('gaussian-blur', NULL, 4),
  ('stanford-crypto-aes', NULL, 4),
  ('stanford-crypto-pbkdf2', NULL, 4),
  ('stanford-crypto-sha256', NULL, 4),
  ('json-stringify-inspector', NULL, 2),
  ('json-parse-inspector', NULL, 2),
  ('bigint-noble-bls12-381', NULL, 1),
  ('bigint-noble-secp256k1', NULL, 4),
  ('bigint-noble-ed25519', NULL, 4),
  ('bigint-paillier', NULL, 2),
  ('bigint-bigdenary', NULL, 16),
  ('proxy-mobx', NULL, 12),
  ('proxy-vue', NULL, 4),
  ('mobx-startup', NULL, 3),
  ('jsdom-d3-startup', NULL, 2),
  ('web-ssr', NULL, 4),
  ('raytrace-public-class-fields', NULL, 4),
  ('raytrace-private-class-fields', NULL, 4),
  ('async-fs', NULL, 6),
  ('sync-fs', NULL, 6),
  ('lazy-collections', NULL, 4),
  ('js-tokens', NULL, 4),
  ('threejs', NULL, 4),
  ('HashSet-wasm', NULL, 4),
  ('quicksort-wasm', NULL, 4),
  ('gcc-loops-wasm', NULL, 4),
  ('tsf-wasm', NULL, 4),
  ('richards-wasm', NULL, 4),
  ('sqlite3-wasm', NULL, 2),
  ('Dart-flute-complex-wasm', NULL, 2),
  ('Dart-flute-todomvc-wasm', NULL, 2),
  ('Kotlin-compose-wasm', NULL, 1),
  ('transformersjs-bert-wasm', NULL, 4),
  ('transformersjs-whisper-wasm', NULL, 1),
  ('argon2-wasm', NULL, 3),
  ('babylonjs-startup-es5', NULL, 4),
  ('babylonjs-startup-es6', NULL, 4),
  ('babylonjs-scene-es5', NULL, 4),
  ('babylonjs-scene-es6', NULL, 4),
  ('bomb-workers', NULL, 4),
  ('segmentation', NULL, 3),
  ('8bitbench-wasm', NULL, 2),
  ('zlib-wasm', NULL, 4),
  ('dotnet-interp-wasm', NULL, 2),
  ('dotnet-aot-wasm', NULL, 2),
  ('j2cl-box2d-wasm', NULL, 4),
  ('prismjs-startup-es6', NULL, 4),
  ('prismjs-startup-es5', NULL, 4),
  ('acorn-wtb', NULL, 2),
  ('babel-wtb', NULL, 2),
  ('babel-minify-wtb', NULL, 2),
  ('babylon-wtb', NULL, 2),
  ('chai-wtb', NULL, 2),
  ('espree-wtb', NULL, 2),
  ('esprima-next-wtb', NULL, 2),
  ('lebab-wtb', NULL, 2),
  ('postcss-wtb', NULL, 2),
  ('prettier-wtb', NULL, 2),
  ('source-map-wtb', NULL, 2),
  ('RelativeTimeFormat-intl', 'intl', 1),
  ('PluralRules-intl', 'intl', 1),
  ('NumberFormat-intl', 'intl', 1),
  ('ListFormat-intl', 'intl', 1),
  ('DateTimeFormat-intl', 'intl', 1),
  ('tagcloud-SP', 'Sunspider', 4),
  ('string-unpack-code-SP', 'Sunspider', 4),
  ('regex-dna-SP', 'Sunspider', 4),
  ('n-body-SP', 'Sunspider', 4),
  ('date-format-xparb-SP', 'Sunspider', 4),
  ('date-format-tofte-SP', 'Sunspider', 4),
  ('crypto-sha1-SP', 'Sunspider', 4),
  ('crypto-md5-SP', 'Sunspider', 4),
  ('crypto-aes-SP', 'Sunspider', 4),
  ('base64-SP', 'Sunspider', 4),
  ('3d-raytrace-SP', 'Sunspider', 4),
  ('3d-cube-SP', 'Sunspider', 4)
)
SELECT name, COALESCE(top_level_name, name) AS top_level_name, worst_case_count FROM data;

-- Find the main thread of the JetStream 3 renderer.
CREATE PERFETTO FUNCTION chrome_jetstream_3_renderer_main_utid()
RETURNS TABLE(
  -- The utid of the JetStream 3 renderer main thread.
  utid LONG
) AS
WITH
  start_event AS (
    SELECT
      name
    FROM _chrome_jetstream_3_benchmark_name
    LIMIT 1
  )
SELECT
  utid
FROM thread_track
WHERE
  id IN (
    SELECT
      track_id
    FROM slice
    JOIN start_event
      USING (name)
    WHERE
      category = 'blink.user_timing'
  );

-- Find JetStream 3 iteration slices.
CREATE PERFETTO VIEW _chrome_jetstream_3_measure_slice AS
SELECT
  id,
  ts,
  dur,
  bench.name,
  bench.top_level_name,
  bench.worst_case_count,
  CAST(SUBSTR(slice.name, INSTR(slice.name, '-iteration-') + 11) AS LONG) AS iteration
FROM slice
JOIN _chrome_jetstream_3_benchmark_name AS bench
  ON slice.name GLOB bench.name || '-iteration-*'
WHERE category = 'blink.user_timing' AND dur > 0;

-- Find WSL slices.
CREATE PERFETTO VIEW _chrome_jetstream_3_wsl_slice AS
SELECT
  id,
  ts,
  dur,
  'WSL' AS name,
  'WSL' AS top_level_name,
  0 AS worst_case_count,
  0 AS iteration,
  slice.name AS subtest
FROM slice
WHERE category = 'blink.user_timing'
  AND name GLOB 'WSL-*'
  AND dur > 0;

-- Final list of JetStream 3 measures.
--
-- Classified into:
--   * 'First' (Iteration 0),
--   * 'Average' (Iteration 1..N)
--   * 'Worst' (slowest iterations).
--
-- Note: 'Average' includes all iterations except the first and the worst ones.
-- In the actual benchmark scoring, 'Average' includes 'Worst', but the measure
-- table avoids listing the same slice twice. For WSL, the subtest name is the
-- slice name (e.g., 'WSL-stdlib').
CREATE PERFETTO TABLE chrome_jetstream_3_measure (
  -- The slice id.
  id LONG,
  -- The slice timestamp.
  ts TIMESTAMP,
  -- The slice duration.
  dur DURATION,
  -- The leaf benchmark name.
  name STRING,
  -- The top-level benchmark group name.
  top_level_name STRING,
  -- The iteration number.
  iteration LONG,
  -- The subtest classification ('First', 'Average', 'Worst', or WSL subtest name).
  subtest STRING
) AS
WITH
  iteration_ranked AS (
    SELECT
      id,
      ts,
      dur,
      name,
      top_level_name,
      iteration,
      worst_case_count,
      RANK() OVER (PARTITION BY name ORDER BY dur DESC) as rank_within_name
    FROM _chrome_jetstream_3_measure_slice
    WHERE iteration > 0
  )
  -- Worst: The slowest N iterations (excluding the first).
  SELECT
    id,
    ts,
    dur,
    name,
    top_level_name,
    iteration,
    'Worst' AS subtest
  FROM iteration_ranked
  WHERE rank_within_name <= worst_case_count
UNION ALL
  -- Average: All iterations (excluding the first and worst ones).
  SELECT
    id,
    ts,
    dur,
    name,
    top_level_name,
    iteration,
    'Average' AS subtest
  FROM iteration_ranked
  WHERE rank_within_name > worst_case_count
UNION ALL
  -- First: The first iteration (iteration 0).
  SELECT
    id,
    ts,
    dur,
    name,
    top_level_name,
    iteration,
    'First' AS subtest
  FROM _chrome_jetstream_3_measure_slice
  WHERE iteration = 0
UNION ALL
  -- WSL: Specific WSL slices.
  SELECT
    id,
    ts,
    dur,
    name,
    top_level_name,
    iteration,
    subtest
  FROM _chrome_jetstream_3_wsl_slice;

-- Calculate score for each subtest classification.
--
-- A subtest score is 5000 / arithmetic mean of durations in milliseconds.
CREATE PERFETTO VIEW _chrome_jetstream_3_subtest_score AS
WITH
  expanded_measure AS (
    SELECT * FROM chrome_jetstream_3_measure
    UNION ALL
    -- Include 'Worst' iterations in 'Average' calculation.
    SELECT
      id,
      ts,
      dur,
      name,
      top_level_name,
      iteration,
      'Average' AS subtest
    FROM chrome_jetstream_3_measure
    WHERE subtest = 'Worst'
  )
SELECT
  top_level_name,
  name,
  subtest,
  5000.0 / (AVG(dur) / 1000000.0) AS score
FROM expanded_measure
GROUP BY top_level_name, name, subtest;

-- Calculate the score for each leaf benchmark.
--
-- A benchmark's score is the geometric mean of its subtest scores (First,
-- Average, Worst).
--
-- Note: some benchmarks might not have exactly these three subtests, e.g. WSL.
CREATE PERFETTO VIEW _chrome_jetstream_3_leaf_benchmark_score AS
SELECT
  top_level_name,
  name,
  EXP(AVG(LN(score))) AS score
FROM _chrome_jetstream_3_subtest_score
GROUP BY top_level_name, name;

-- Calculate the score for each top-level benchmark.
---
-- If a benchmark is grouped, its score is the geometric mean of its children's
-- scores. Otherwise, for ungrouped benchmarks, their score is the top-level
-- score.
CREATE PERFETTO TABLE chrome_jetstream_3_benchmark_score (
  -- The top-level benchmark group name.
  top_level_name STRING,
  -- The calculated score for the top-level benchmark.
  score DOUBLE
) AS
SELECT
  top_level_name,
  EXP(AVG(LN(score))) AS score
FROM _chrome_jetstream_3_leaf_benchmark_score
GROUP BY top_level_name;

-- Overall JetStream 3 score.
--
-- The final score is the geometric mean of all top-level benchmark scores.
CREATE PERFETTO FUNCTION chrome_jetstream_3_score()
-- The overall JetStream 3 score.
RETURNS DOUBLE AS
SELECT
  EXP(AVG(LN(score)))
FROM chrome_jetstream_3_benchmark_score;
