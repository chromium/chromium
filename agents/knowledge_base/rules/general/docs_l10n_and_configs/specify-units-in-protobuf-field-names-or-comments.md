---
name: general/specify-units-in-protobuf-field-names-or-comments
rubric: readability
severity: optional
---

**Rule.** When defining Protocol Buffer (`.proto`) fields that represent
physical, dimensional, temporal, or capacity quantities (such as coordinates,
offsets, durations, latencies, or memory sizes), explicitly specify the unit of
measurement (such as `_px`, `_dip`, `_ms`, `_us`, `_bytes`, `_millis`). Include
the unit as a suffix in the field identifier or document it clearly in the
field's doc comment.

**Why.** Protobuf primitive numeric types (such as `float`, `double`, `int32`,
`int64`) lack unit information in the type system. Without explicit unit
suffixes or comments, values can easily be misinterpreted (such as physical
pixels vs. DIPs, or milliseconds vs. microseconds), causing silent scaling bugs
and integration errors across producer and consumer platforms.

## Examples

Bad:

```cpp
message TopControlsOffset {
  optional float viz_y_offset = 1;
  optional float browser_y_offset = 2;
  optional int64 scroll_delta_time = 3;
}
```

Good:

```cpp
message TopControlsOffset {
  // Offset in physical pixels.
  optional float viz_y_offset_px = 1;
  optional float browser_y_offset_px = 2;
  // Duration delta in milliseconds.
  optional int64 scroll_delta_time_ms = 3;
}
```
