---
name: base/document-perfetto-sql-functions-and-tables
rubric: readability
severity: nit
---

**Rule.** In Perfetto SQL standard library modules, precede CREATE PERFETTO
FUNCTION, TABLE, VIEW, and MACRO definitions with explanatory doc comments.
Describe what the function, table, or metric computes, detail each parameter and
return value semantics, and explicitly specify the exact entity, event, and
milestone occurrence being measured rather than using vague shorthand.

**Why.** Perfetto standard library functions, tables, and views act as public
building blocks for trace metrics and dashboards. Vague labels (such as 'End
request') obscure whether network, IPC, or document requests are tracked, while
explicit comments clarifying purpose, parameters, and exact event entities allow
query authors to reuse and interpret metrics without reverse-engineering query
internals.

## Examples

Bad:

```cpp
CREATE PERFETTO FUNCTION _chrome_get_next_presentation_time_by_pid(
    ts INT, pid INT)
RETURNS INT
AS
SELECT MIN(a.ts + a.dur) AS ts
FROM process_slice s, ancestor_slice(s.id) a;

-- End request for each page
CREATE PERFETTO TABLE _chrome_loadline2_end_request (
  ts TIMESTAMP,
  page STRING
) AS ...
```

Good:

```cpp
-- Gets the presentation time of the first frame committed after ts
-- in the renderer process with pid.
CREATE PERFETTO FUNCTION _chrome_get_next_presentation_time_by_pid(
    ts INT, pid INT)
RETURNS INT
AS
SELECT MIN(a.ts + a.dur) AS ts
FROM process_slice s, ancestor_slice(s.id) a;

-- Finish timestamp for the first network request for each page
CREATE PERFETTO TABLE _chrome_loadline2_end_request (
  ts TIMESTAMP,
  page STRING
) AS ...
```
