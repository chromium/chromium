---
name: general/name-protobuf-fields-after-semantic-concepts
rubric: readability
severity: nit
---

**Rule.** Name protobuf fields after the semantic data or concept they convey
rather than after platform-specific source types, APIs, or subsystems. When
platform-specific messages supply general data, use a platform-neutral field
identifier and document platform-specific constraints in an inline comment.

**Why.** Naming fields after platform-specific source types or APIs couples the
message schema to underlying implementation details and makes shared schemas
harder to understand. Using semantic, platform-agnostic field names ensures
schemas remain clear, intuitive, and forward-compatible if other platforms
supply equivalent data later.

## Examples

Bad:

```cpp
message ChromeGraphicsPipeline {
  optional AndroidChoreographerFrameCallbackData
      android_choreographer_frame_callback_data = 11;
}
```

Good:

```cpp
message ChromeGraphicsPipeline {
  // Optional frame data containing possible latch and present frame deadlines.
  // Set on Android devices.
  optional AndroidChoreographerFrameCallbackData possible_deadlines = 11;
}
```
