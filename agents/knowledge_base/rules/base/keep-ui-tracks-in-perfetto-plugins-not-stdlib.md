---
name: base/keep-ui-tracks-in-perfetto-plugins-not-stdlib
rubric: design
severity: should_fix
---

**Rule.** In the PerfettoSQL standard library (`base/tracing/stdlib/`), do not
define visualization, UI track, or UI slice tables. Expose only reusable
semantic data models, metrics, and stage breakdowns in the standard library, and
implement visualization logic and UI tracks in Perfetto plugins.

**Why.** The PerfettoSQL standard library serves as a reusable, platform-wide
data layer for automated tests, metrics, and data analysis tools. Formatting
data specifically for UI presentation or visual tracks couples the standard
library to UI details and bloats the stdlib schema; visual presentation logic
belongs in Perfetto plugins.

## Examples

Bad:

```cpp
-- base/tracing/stdlib/chrome/loadline_2_track.sql
-- A UI track for LoadLine 2 traces that formats slices for UI display
CREATE PERFETTO TABLE chrome_loadline2_ui_slices (
  page STRING,
  name STRING,
  ts TIMESTAMP,
  dur DURATION
) AS
SELECT page, stage_name AS name, ts, dur
FROM _loadline2_events;
```

Good:

```cpp
-- Expose reusable semantic stages in stdlib; implement UI tracks in Perfetto plugins:
-- base/tracing/stdlib/chrome/loadline_2.sql
CREATE PERFETTO TABLE chrome_loadline2_stages (
  page STRING,
  stage_name STRING,
  ts TIMESTAMP,
  dur DURATION
) AS
SELECT page, stage_name, ts, dur
FROM _loadline2_events;
```
