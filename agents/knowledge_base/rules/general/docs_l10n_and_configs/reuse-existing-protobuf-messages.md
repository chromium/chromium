---
name: general/reuse-existing-protobuf-messages
rubric: design
severity: should_fix
---

**Rule.** When defining fields or submessages in protobuf schemas (`.proto`),
reuse or generalize existing protobuf message types that represent equivalent
data structures rather than declaring duplicate message definitions. If an
existing message needs additional fields or minor adjustments to cover the use
case, extend or generalize the existing schema instead of duplicating it.

**Why.** Duplicate message schemas bloat protobuf definitions, create redundant
maintenance burdens, and make cross-event querying, aggregation, and analysis in
downstream telemetry and trace processing tools (such as Perfetto Trace
Processor) significantly more complex.

## Examples

Bad:

```cpp
// In chrome_track_event.proto:
message ChromeGraphicsPipeline {
  message PossibleDeadlines {
    message Deadline {
      optional int64 latch_time = 1;
      optional int64 present_time = 2;
    }
    repeated Deadline deadlines = 1;
    optional int64 preferred_index = 2;
  }
  optional PossibleDeadlines possible_deadlines = 11;
}
```

Good:

```cpp
// In chrome_track_event.proto: reuse existing message representing choreographer deadlines
import "protos/perfetto/trace/android/android_game_intervention_list.proto";

message ChromeGraphicsPipeline {
  optional AndroidChoreographerFrameCallbackData
      android_choreographer_frame_callback_data = 11;
}
```
