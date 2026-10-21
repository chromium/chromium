---
name: base/prefix-internal-perfetto-sql-tables-with-underscore
rubric: readability
severity: should_fix
---

**Rule.** In Perfetto SQL stdlib modules, prefix internal and intermediate
tables, views, functions, and macros with a leading underscore (`_`). Reserve
unprefixed names strictly for the documented public tables, views, functions,
and macros intended for external consumers.

**Why.** Perfetto SQL stdlib modules form a public API surface for trace
queries, metrics, and dashboards. Prefixing internal helper tables and
intermediate query steps with an underscore prevents polluting the stdlib
namespace and stops external callers from taking unintended dependencies on
implementation details.

## Examples

Bad:

```cpp
-- Intermediate helper table exposed publicly without leading underscore
CREATE PERFETTO TABLE chrome_loadline2_story_start (
  page STRING,
  story_start TIMESTAMP
) AS
SELECT page, MIN(ts) AS story_start
FROM chrome_loadline2_events
GROUP BY page;
```

Good:

```cpp
-- Intermediate helper table marked internal with leading underscore
CREATE PERFETTO TABLE _chrome_loadline2_story_start (
  page STRING,
  story_start TIMESTAMP
) AS
SELECT page, MIN(ts) AS story_start
FROM _chrome_loadline2_events
GROUP BY page;

-- Only the intended public API table lacks the leading underscore
CREATE PERFETTO TABLE chrome_loadline2_stages AS
SELECT ...;
```
