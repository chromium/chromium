---
name: general/pair-anomaly-reasons-with-severity-metrics-in-protos
rubric: testing
severity: optional
---

**Rule.** When defining Protocol Buffer schemas or telemetry event data
structures for performance anomalies (such as scroll jank, dropped frames, or
pipeline stalls), pair categorical failure reasons with quantitative severity
metrics (such as missed VSync counts, frame drops, or delay durations) in a
structured message rather than emitting bare reason enums alone.

**Why.** Categorical anomaly enums only describe why a performance failure
occurred, without indicating how severely the user experience was impacted.
Emitting quantitative metrics alongside reasons enables telemetry queries and
Perfetto SQL tables to filter or aggregate by magnitude (such as isolating
multi-frame janks from single VSync slips) without expensive heuristics or loss
of precision.

## Examples

Bad:

```cpp
// BAD: Emitting only categorical reasons makes it impossible to filter by jank severity.
enum JankReason {
  JANK_REASON_UNSPECIFIED = 0;
  MISSED_VSYNC_DUE_TO_DECELERATING_INPUT = 1;
  MISSED_VSYNC_DURING_FAST_SCROLL = 2;
}

message ScrollJankEvent {
  repeated JankReason jank_reasons = 1;
}
```

Good:

```cpp
// GOOD: Pair each anomaly reason with a quantitative severity metric (e.g. missed VSyncs).
message MissedVsyncsForJankReason {
  optional JankReason jank_reason = 1;
  optional int32 missed_vsyncs = 2;
}

message ScrollJankEvent {
  repeated MissedVsyncsForJankReason missed_vsyncs_per_jank_reason = 1;
}
```
