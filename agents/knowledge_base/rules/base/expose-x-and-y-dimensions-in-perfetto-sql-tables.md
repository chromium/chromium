---
name: base/expose-x-and-y-dimensions-in-perfetto-sql-tables
rubric: correctness
severity: should_fix
---

**Rule.** When defining Perfetto SQL stdlib tables or views for input, gesture,
or scroll event deltas, extract and expose both X and Y coordinates (delta_x and
delta_y). Maintain coordinate dimension parity across related pipeline stages
such as raw input and presented frame deltas.

**Why.** Input and presentation events track 2D motion across both axes.
Omitting the horizontal dimension (delta_x) limits multi-axis gesture analysis
and creates schema discrepancies with downstream presentation tables that expose
both axes.

## Examples

Bad:

```cpp
CREATE PERFETTO TABLE chrome_scroll_input_deltas(
  scroll_update_id LONG,
  delta_y DOUBLE
) AS
SELECT
  EXTRACT_ARG(arg_set_id, 'scroll_deltas.trace_id') AS scroll_update_id,
  EXTRACT_ARG(arg_set_id, 'scroll_deltas.original_delta_y') AS delta_y
FROM slice
WHERE slice.name = 'TranslateAndScaleWebInputEvent';
```

Good:

```cpp
CREATE PERFETTO TABLE chrome_scroll_input_deltas(
  scroll_update_id LONG,
  delta_x DOUBLE,
  delta_y DOUBLE
) AS
SELECT
  EXTRACT_ARG(arg_set_id, 'scroll_deltas.trace_id') AS scroll_update_id,
  EXTRACT_ARG(arg_set_id, 'scroll_deltas.original_delta_x') AS delta_x,
  EXTRACT_ARG(arg_set_id, 'scroll_deltas.original_delta_y') AS delta_y
FROM slice
WHERE slice.name = 'TranslateAndScaleWebInputEvent';
```
