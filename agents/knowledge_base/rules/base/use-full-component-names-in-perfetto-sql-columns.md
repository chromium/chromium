---
name: base/use-full-component-names-in-perfetto-sql-columns
rubric: readability
severity: optional
---

**Rule.** In Perfetto SQL stdlib table and view schemas (`CREATE PERFETTO TABLE`
/ `CREATE PERFETTO VIEW`), use full, unabbreviated component and pipeline stage
names (such as `input_reader` or `input_dispatcher`) rather than shortened or
colloquial forms (such as `reader` or `dispatch`) when defining timestamp,
duration, or stage column names.

**Why.** Perfetto standard library table columns serve as public SQL schema
interfaces across queries, dashboards, and metric definitions. Shortening or
abbreviating component and stage names introduces ambiguity about which
subsystem is referenced and creates inconsistency with related pipeline stage
columns.

## Examples

Bad:

```cpp
CREATE PERFETTO TABLE chrome_scroll_update_input_pipeline (
  scroll_update_id LONG,
  generation_to_reader_end_dur DURATION,
  generation_to_dispatch_end_dur DURATION
);
```

Good:

```cpp
CREATE PERFETTO TABLE chrome_scroll_update_input_pipeline (
  scroll_update_id LONG,
  generation_to_input_reader_end_dur DURATION,
  generation_to_input_dispatcher_end_dur DURATION
);
```
