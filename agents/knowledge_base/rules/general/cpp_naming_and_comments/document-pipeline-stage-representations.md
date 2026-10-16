---
name: general/document-pipeline-stage-representations
rubric: readability
severity: optional
---

**Rule.** When declaring schema tables, columns, views, protobuf messages/enums,
or structs representing data and states across different stages of a processing
pipeline (such as raw input vs. presented frame deltas, or input generation vs.
presentation timestamps), explicitly document their semantic distinctions,
relationships, and data sources in doc comments. When multiple fields or enum
variants capture similar states (such as extrapolation, estimation, or synthetic
status) across orthogonal pipeline dimensions, document their orthogonality and
provide examples of how they combine independently.

**Why.** Without explicit doc comments explaining how related data entities,
columns, or enum variants differ across pipeline stages (e.g., raw hardware
signals vs. application-computed offsets) or combine across orthogonal
dimensions (e.g., input vs. presentation extrapolation), consumers and reviewers
cannot determine how data flows through the pipeline and may assume superficial
redundancy or mutual exclusion.

## Examples

Bad:

```cpp
// Variant 1: Undocumented distinctions between pipeline stage tables/columns
CREATE PERFETTO TABLE chrome_scroll_presented_deltas(
  scroll_update_id LONG,
  total_input_delta_y DOUBLE,
  presented_scrolled_delta_y DOUBLE
) AS ...;

// Variant 2: Undocumented orthogonal enum variants across pipeline stages
enum FirstScrollUpdateType {
  SYNTHETIC_WITH_EXTRAPOLATED_INPUT_GENERATION_TIMESTAMP = 2;
  SYNTHETIC_WITHOUT_EXTRAPOLATED_INPUT_GENERATION_TIMESTAMP = 3;
}
optional FirstScrollUpdateType first_scroll_update_type = 10;

enum DamageType {
  NON_DAMAGING_WITH_EXTRAPOLATED_PRESENTATION_TIMESTAMP = 2;
  NON_DAMAGING_WITHOUT_EXTRAPOLATED_PRESENTATION_TIMESTAMP = 3;
}
optional DamageType damage_type = 11;
```

Good:

```cpp
// Variant 1: Explicitly document stage semantics and data sources
-- Tracks page offset delta for each frame after input is processed.
-- `chrome_scroll_input_deltas` tracks raw OS input event deltas.
CREATE PERFETTO TABLE chrome_scroll_presented_deltas(
  scroll_update_id LONG,
  -- Sum of OS input deltas for all scroll updates in this frame.
  total_input_delta_y DOUBLE,
  -- Change in page offset computed by Chrome for this frame.
  presented_scrolled_delta_y DOUBLE
) AS ...;

// Variant 2: Document orthogonal dimensions and independent combinations
// Note: `first_scroll_update_type` and `damage_type` are orthogonal: the former
// tracks input generation (synthetic scroll updates), while the latter tracks
// presentation (damaging frames). A frame can have either or both extrapolated.
enum DamageType {
  NON_DAMAGING_WITH_EXTRAPOLATED_PRESENTATION_TIMESTAMP = 2;
  NON_DAMAGING_WITHOUT_EXTRAPOLATED_PRESENTATION_TIMESTAMP = 3;
}
optional DamageType damage_type = 11;
```
