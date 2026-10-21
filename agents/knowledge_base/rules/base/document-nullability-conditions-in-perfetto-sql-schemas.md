---
name: base/document-nullability-conditions-in-perfetto-sql-schemas
rubric: readability
severity: nit
---

**Rule.** In PerfettoSQL standard library table and view definitions
(`CREATE PERFETTO TABLE` / `CREATE PERFETTO VIEW` in `base/tracing/stdlib/`),
thoroughly document column schemas in schema comments and type annotations:

1. For ID columns, document the referenced target table and column (e.g.,
   `-- Can be joined with \`\<table.column>\``) or use `JOINID(...)\` type
   annotations where supported, rather than leaving generic ID descriptions.
2. For conditionally populated or nullable columns, explicitly document the
   conditions under which columns can be NULL or populated (such as required
   trace or atrace categories, platform constraints, event types, or pipeline
   stages).

**Why.** PerfettoSQL standard library schemas are exported into public
documentation and validated by `tools/tracing/check_stdlib`. Documenting join
targets and `JOINID` relationships allows query authors to combine tables
without reverse-engineering schemas. Clarifying nullability conditions ensures
query authors write correct null-handling logic and understand required trace
recording configurations.

## Examples

Bad:

```cpp
CREATE PERFETTO TABLE chrome_android_input (
  -- Frame ID.
  frame_id LONG,
  -- End timestamp for the InputReader step (see android_input.sql).
  input_reader_processing_end_ts TIMESTAMP,
  -- Tag of the scroll jank cause.
  tag STRING
);
```

Good:

```cpp
CREATE PERFETTO TABLE chrome_android_input (
  -- Frame ID. Can be joined with `chrome_scroll_frame_info_v4.id`.
  frame_id JOINID(slice.id),
  -- End timestamp for the InputReader step (see android_input.sql).
  -- Only populated when atrace 'input' category is enabled.
  input_reader_processing_end_ts TIMESTAMP,
  -- Tag of the scroll jank cause.
  tag STRING
);
```
