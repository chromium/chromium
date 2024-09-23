-- Copyright 2023 The Chromium Authors
-- Use of this source code is governed by a BSD-style license that can be
-- found in the LICENSE file.

INCLUDE PERFETTO MODULE slices.with_context;

-- Chrome web content interactions (InteractionToFirstPaint), including
-- associated high-level metrics and properties.
--
-- Multiple events may occur for the same interaction; each row in this table
-- represents the primary (longest) event for the interaction.
--
-- Web content interactions are discrete, as opposed to sustained (e.g.
-- scrolling); and only occur with the web content itself, as opposed to other
-- parts of Chrome (e.g. omnibox). Interaction events include taps, clicks,
-- keyboard input (typing), and drags.
CREATE PERFETTO TABLE chrome_web_content_interactions(
  -- Unique id for this interaction.
  id INT,
  -- Start timestamp of the event. Because multiple events may occur for the
  -- same interaction, this is the start timestamp of the longest event.
  ts INT,
  -- Duration of the event. Because multiple events may occur for the same
  -- interaction, this is the duration of the longest event.
  dur INT,
  -- The interaction type.
  interaction_type STRING,
  -- The total duration of all events that occurred for the same interaction.
  total_duration_ms INT,
  -- The process id this event occurred on.
  renderer_upid INT
) AS
SELECT
  id,
  ts,
  dur,
  EXTRACT_ARG(arg_set_id, 'web_content_interaction.type') AS interaction_type,
  EXTRACT_ARG(
    arg_set_id,
    'web_content_interaction.total_duration_ms'
  ) AS total_duration_ms,
  upid AS renderer_upid
FROM process_slice
WHERE name = 'Web Interaction';
