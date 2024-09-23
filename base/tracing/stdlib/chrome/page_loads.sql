-- Copyright 2023 The Chromium Authors
-- Use of this source code is governed by a BSD-style license that can be
-- found in the LICENSE file.

-- TODO(b/306300843): The recorded navigation ids are not guaranteed to be
-- unique within a trace; they are only guaranteed to be unique within a single
-- chrome instance. Chrome instance id needs to be recorded, and used here in
-- combination with navigation id to uniquely identify page load metrics.

INCLUDE PERFETTO MODULE slices.with_context;

CREATE PERFETTO VIEW _fcp_metrics AS
SELECT
  ts,
  dur,
  EXTRACT_ARG(arg_set_id, 'page_load.navigation_id') AS navigation_id,
  EXTRACT_ARG(arg_set_id, 'page_load.url') AS url,
  upid AS browser_upid
FROM process_slice
WHERE name = 'PageLoadMetrics.NavigationToFirstContentfulPaint';

CREATE PERFETTO FUNCTION _page_load_metrics(event_name STRING)
RETURNS TABLE(
  ts LONG,
  dur LONG,
  navigation_id INT,
  browser_upid INT
) AS
SELECT
  ts,
  dur,
  EXTRACT_ARG(arg_set_id, 'page_load.navigation_id')
    AS navigation_id,
  upid AS browser_upid
FROM process_slice
WHERE name = $event_name;

-- Chrome page loads, including associated high-level metrics and properties.
CREATE PERFETTO TABLE chrome_page_loads(
  -- ID of the navigation and Chrome browser process; this combination is
  -- unique to every individual navigation.
  id INT,
  -- ID of the navigation associated with the page load (i.e. the cross-document
  -- navigation in primary main frame which created this page's main document).
  -- Also note that navigation_id is specific to a given Chrome browser process,
  -- and not globally unique.
  navigation_id INT,
  -- Timestamp of the start of navigation.
  navigation_start_ts INT,
  -- Duration between the navigation start and the first contentful paint event
  -- (web.dev/fcp).
  fcp INT,
  -- Timestamp of the first contentful paint.
  fcp_ts INT,
  -- Duration between the navigation start and the largest contentful paint event
  -- (web.dev/lcp).
  lcp INT,
  -- Timestamp of the largest contentful paint.
  lcp_ts INT,
  -- Timestamp of the DomContentLoaded event:
  -- https://developer.mozilla.org/en-US/docs/Web/API/Document/DOMContentLoaded_event
  dom_content_loaded_event_ts INT,
  -- Timestamp of the window load event:
  -- https://developer.mozilla.org/en-US/docs/Web/API/Window/load_event
  load_event_ts INT,
  -- Timestamp of the page self-reporting as fully loaded through the
  -- performance.mark('mark_fully_loaded') API.
  mark_fully_loaded_ts INT,
  -- Timestamp of the page self-reporting as fully visible through the
  -- performance.mark('mark_fully_visible') API.
  mark_fully_visible_ts INT,
  -- Timestamp of the page self-reporting as fully interactive through the
  -- performance.mark('mark_interactive') API.
  mark_interactive_ts INT,
  -- URL at the page load event.
  url STRING,
  -- The unique process id (upid) of the browser process where the page load occurred.
  browser_upid INT
) AS
SELECT
  ROW_NUMBER() OVER(ORDER BY fcp.ts) AS id,
  fcp.navigation_id,
  fcp.ts AS navigation_start_ts,
  fcp.dur AS fcp,
  fcp.ts + fcp.dur AS fcp_ts,
  lcp.dur AS lcp,
  lcp.dur + lcp.ts AS lcp_ts,
  load_fired.ts AS dom_content_loaded_event_ts,
  start_load.ts AS load_event_ts,
  timing_loaded.ts AS mark_fully_loaded_ts,
  timing_visible.ts AS mark_fully_visible_ts,
  timing_interactive.ts AS mark_interactive_ts,
  fcp.url,
  fcp.browser_upid
FROM _fcp_metrics fcp
LEFT JOIN
  _page_load_metrics('PageLoadMetrics.NavigationToLargestContentfulPaint') lcp
    USING (navigation_id, browser_upid)
LEFT JOIN
  _page_load_metrics('PageLoadMetrics.NavigationToDOMContentLoadedEventFired') load_fired
    USING (navigation_id, browser_upid)
LEFT JOIN
  _page_load_metrics('PageLoadMetrics.NavigationToMainFrameOnLoad') start_load
    USING (navigation_id, browser_upid)
LEFT JOIN
  _page_load_metrics('PageLoadMetrics.UserTimingMarkFullyLoaded') timing_loaded
    USING (navigation_id, browser_upid)
LEFT JOIN
  _page_load_metrics('PageLoadMetrics.UserTimingMarkFullyVisible') timing_visible
    USING (navigation_id, browser_upid)
LEFT JOIN
  _page_load_metrics('PageLoadMetrics.UserTimingMarkInteractive') timing_interactive
    USING (navigation_id, browser_upid);
