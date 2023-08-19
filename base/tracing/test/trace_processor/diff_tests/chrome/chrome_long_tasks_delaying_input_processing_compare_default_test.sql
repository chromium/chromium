-- Copyright 2023 The Chromium Authors
-- Use of this source code is governed by a BSD-style license that can be
-- found in the LICENSE file.

-- This test runs the on a trace that includes both LongTask tracking traces
-- and top-level traces. This test verifies that the default input processing
-- delay metric can be calculated while both scenarios are enabled. The output
-- should be consistent (same tasks) as the test for the LongTask version of the
-- metric - chrome_long_tasks_delaying_input_processing_test.sql

SELECT RUN_METRIC(
  'chrome/chrome_tasks_delaying_input_processing.sql',
  'duration_causing_jank_ms', '4'
);

SELECT
  full_name,
  duration_ms,
  slice_id
FROM chrome_tasks_delaying_input_processing
ORDER BY slice_id;