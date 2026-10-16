---
name: general/use-explicit-on-for-aliased-sql-joins
rubric: readability
severity: should_fix
---

**Rule.** In SQL and PerfettoSQL queries, when joining generic entity tables
(such as `thread`, `process`, `slice`, or `args`) representing specific semantic
roles, alias each table with a descriptive role-specific name (e.g.
`thread AS browser_main_thread`, `process AS browser_process`). Additionally,
specify explicit `ON <alias1>.<col> = <alias2>.<col>` join conditions with fully
qualified column names instead of `USING(<col>)`.

**Why.** Leaving generic tables unaliased obscures which entity role (such as
browser, renderer, or GPU) is being inspected. Similarly, `USING(<col>)` relies
on implicit column resolution across preceding tables in the `FROM` clause;
using descriptive role aliases combined with explicit `ON` conditions prevents
ambiguous column resolution and keeps queries resilient against schema changes.

## Examples

Bad:

```cpp
-- Unaliased generic tables and USING obscure entity roles and column sources.
JOIN thread ON info.browser_utid = thread.utid
JOIN process USING(upid)
```

Good:

```cpp
-- Role-specific aliases and explicit ON conditions disambiguate entity relationships.
LEFT JOIN thread AS browser_main_thread
  ON browser_utid = browser_main_thread.utid
JOIN process AS browser_process
  ON browser_process.upid = browser_main_thread.upid
```
