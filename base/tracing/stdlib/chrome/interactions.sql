-- Copyright 2023 The Chromium Authors
-- Use of this source code is governed by a BSD-style license that can be
-- found in the LICENSE file.

-- This file specifies common metrics/tables for critical user interactions. It
-- is expected to be in flux as metrics are added across different CUI types.
-- Currently we only track Chrome page loads and their associated metrics.

INCLUDE PERFETTO MODULE chrome.page_loads;
INCLUDE PERFETTO MODULE chrome.scroll_interactions;
INCLUDE PERFETTO MODULE chrome.startups;
INCLUDE PERFETTO MODULE chrome.web_content_interactions;

-- All critical user interaction events, including type and table with
-- associated metrics.
CREATE PERFETTO TABLE chrome_interactions(
  -- Identifier of the interaction; this is not guaranteed to be unique to the table -
  -- rather, it is unique within an individual interaction type. Combine with type to get
  -- a unique identifier in this table.
  scoped_id INT,
  -- Type of this interaction, which together with scoped_id uniquely identifies this
  -- interaction. Also corresponds to a SQL table name containing more details specific
  -- to this type of interaction.
  type STRING,
  -- Interaction name - e.g. 'PageLoad', 'Tap', etc. Interactions will have unique metrics
  -- stored in other tables.
  name STRING,
  -- Timestamp of the CUI event.
  ts INT,
  -- Duration of the CUI event.
  dur INT
) AS
SELECT
  id AS scoped_id,
  'chrome_page_loads' AS type,
  'PageLoad' AS name,
  navigation_start_ts AS ts,
  IFNULL(lcp, fcp) AS dur
FROM chrome_page_loads
UNION ALL
SELECT
  id AS scoped_id,
  'chrome_startups' AS type,
  name,
  startup_begin_ts AS ts,
  CASE
    WHEN first_visible_content_ts IS NOT NULL
      THEN first_visible_content_ts - startup_begin_ts
    ELSE 0
  END AS dur
FROM chrome_startups
UNION ALL
SELECT
  id AS scoped_id,
  'chrome_web_content_interactions' AS type,
  'InteractionToFirstPaint' AS name,
  ts,
  dur
FROM chrome_web_content_interactions
UNION ALL
SELECT
  id AS scoped_id,
  'chrome_scroll_interactions' AS type,
  'Scroll' AS name,
  ts,
  dur
FROM chrome_scroll_interactions;
