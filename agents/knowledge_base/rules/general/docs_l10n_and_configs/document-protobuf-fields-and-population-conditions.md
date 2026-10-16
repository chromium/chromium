---
name: general/document-protobuf-fields-and-population-conditions
rubric: readability
severity: nit
---

**Rule.** When adding or modifying fields in Protocol Buffer schemas (`.proto`),
document what each field represents and when, where, or under what conditions it
is populated. Explicitly note any platform restrictions, producer preconditions,
or lifecycle constraints in the field's doc comment.

**Why.** Protocol buffer fields—especially optional fields in telemetry, IPC, or
trace schemas—are often populated only on specific platforms or during
particular event flows. Without doc comments clarifying field semantics and
population lifecycle, consumers cannot determine when data is present or how to
interpret missing values.

## Examples

Bad:

```cpp
message ChromeGraphicsPipeline {
  optional AndroidChoreographerFrameCallbackData possible_deadlines = 11;
}
```

Good:

```cpp
message ChromeGraphicsPipeline {
  // Frame data containing possible latch and present frame deadlines.
  // Only populated on Android devices during Choreographer frame callbacks.
  optional AndroidChoreographerFrameCallbackData possible_deadlines = 11;
}
```
