-- Copyright 2024 The Chromium Authors
-- Use of this source code is governed by a BSD-style license that can be
-- found in the LICENSE file.

INCLUDE PERFETTO MODULE slices.with_context;

-- Processing steps of the Chrome input pipeline.
CREATE PERFETTO TABLE _chrome_input_pipeline_steps_no_input_type(
  -- Id of this Chrome input pipeline (LatencyInfo).
  latency_id INT,
  -- Slice id
  slice_id INT,
  -- The step timestamp.
  ts INT,
  -- Step duration.
  dur INT,
  -- Utid of the thread.
  utid INT,
  -- Step name (ChromeLatencyInfo.step).
  step STRING,
  -- Input type.
  input_type STRING
) AS
SELECT
  EXTRACT_ARG(thread_slice.arg_set_id, 'chrome_latency_info.trace_id') AS latency_id,
  id AS slice_id,
  ts,
  dur,
  utid,
  EXTRACT_ARG(thread_slice.arg_set_id, 'chrome_latency_info.step') AS step,
  EXTRACT_ARG(thread_slice.arg_set_id, 'chrome_latency_info.input_type') AS input_type
FROM
  thread_slice
WHERE
  step IS NOT NULL
  AND latency_id != -1
ORDER BY slice_id, ts;

-- Each row represents one input pipeline.
CREATE PERFETTO TABLE chrome_inputs(
  -- Id of this Chrome input pipeline (LatencyInfo).
  latency_id INT,
   -- Input type.
  input_type STRING
) AS
SELECT
  -- Id of this Chrome input pipeline (LatencyInfo).
  latency_id,
  -- MIN selects the first non-null value.
  MIN(input_type) as input_type
FROM _chrome_input_pipeline_steps_no_input_type
WHERE latency_id != -1
GROUP BY latency_id;

-- Since not all steps have associated input type (but all steps
-- for a given latency id should have the same input type),
-- populate input type for steps where it would be NULL.
CREATE PERFETTO TABLE chrome_input_pipeline_steps(
  -- Id of this Chrome input pipeline (LatencyInfo).
  latency_id INT,
  -- Slice id
  slice_id INT,
  -- The step timestamp.
  ts INT,
  -- Step duration.
  dur INT,
  -- Utid of the thread.
  utid INT,
  -- Step name (ChromeLatencyInfo.step).
  step STRING,
  -- Input type.
  input_type STRING
) AS
SELECT
  latency_id,
  slice_id,
  ts,
  dur,
  utid,
  step,
  chrome_inputs.input_type AS input_type
FROM
  chrome_inputs
LEFT JOIN
  _chrome_input_pipeline_steps_no_input_type
  USING (latency_id)
WHERE chrome_inputs.input_type IS NOT NULL;
