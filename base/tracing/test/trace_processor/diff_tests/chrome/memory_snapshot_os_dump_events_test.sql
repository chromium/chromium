--
-- Copyright 2020 The Android Open Source Project
--
-- Licensed under the Apache License, Version 2.0 (the "License");
-- you may not use this file except in compliance with the License.
-- You may obtain a copy of the License at
--
--     https://www.apache.org/licenses/LICENSE-2.0
--
-- Unless required by applicable law or agreed to in writing, software
-- distributed under the License is distributed on an "AS IS" BASIS,
-- WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
-- See the License for the specific language governing permissions and
-- limitations under the License.
--

SELECT
  p.upid,
  pid,
  p.name,
  timestamp,
  detail_level,
  pf.value AS private_footprint_kb,
  prs.value AS peak_resident_set_kb,
  EXTRACT_ARG(p.arg_set_id, 'is_peak_rss_resettable')
    AS is_peak_rss_resettable
FROM process p
LEFT JOIN memory_snapshot
LEFT JOIN (
  SELECT id, upid
  FROM process_counter_track
  WHERE name = 'chrome.private_footprint_kb'
  ) AS pct_pf
  ON p.upid = pct_pf.upid
LEFT JOIN counter pf 
  ON timestamp = pf.ts AND pct_pf.id = pf.track_id
LEFT JOIN (
  SELECT id, upid
  FROM process_counter_track
  WHERE name = 'chrome.peak_resident_set_kb'
  ) AS pct_prs
  ON p.upid = pct_prs.upid
LEFT JOIN counter prs 
  ON timestamp = prs.ts AND pct_prs.id = prs.track_id
ORDER BY timestamp;
