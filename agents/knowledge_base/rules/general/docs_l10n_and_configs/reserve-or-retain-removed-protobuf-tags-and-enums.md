---
name: general/reserve-or-retain-removed-protobuf-tags-and-enums
rubric: correctness
severity: should_fix
---

**Rule.** In Protocol Buffer (`.proto`) schemas, preserve numeric tag stability
for backward and forward compatibility. When adding fields or enum values,
assign new unused numbers and never renumber existing entries. When removing or
deprecating entries, retain the definition (e.g., `[deprecated = true]` or
`DEPRECATED_` prefix) or declare its tag and name in `reserved` statements.
Conversely, when defining or re-introducing a field, remove its tag number and
name from any `reserved` statements in the enclosing message.

**Why.** Protobuf serializes data using numeric tags on the wire and in
persisted records or traces. Renumbering existing tags or reusing removed tags
causes data corruption and decoder failures across client versions, while
retaining active field tags or names in `reserved` statements violates schema
semantics and causes compiler errors.

## Examples

Bad:

```cpp
// Renumbering existing entries when inserting new enum values:
enum TaskScopeType {
  TASK_SCOPE_SOFT_NAVIGATION = 9;
  TASK_SCOPE_NEW_ENTRY = 10;
  TASK_SCOPE_RESOURCE_TIMING = 11;  // Was 10 in previous releases!
}

// Deleting an enum value or field without reserving tag/name:
message PageLoadMetrics {
  optional uint64 navigation_id = 1;
  // Field 2 deleted outright without reserving
}

// Defining a field while leaving its tag or name in reserved statements:
message ChromeTrackEvent {
  reserved 1045, 1080;
  reserved "accessibility_event";

  optional AccessibilityEvent accessibility_event = 1080;
}
```

Good:

```cpp
// Append new values with new unused IDs without renumbering:
enum TaskScopeType {
  TASK_SCOPE_SOFT_NAVIGATION = 9;
  TASK_SCOPE_RESOURCE_TIMING = 10;
  TASK_SCOPE_NEW_ENTRY = 11;
}

// Option 1: Retain entry with deprecation annotation or prefix:
enum CacheReason {
  CACHE_LIMIT_PRUNED = 67;
  DEPRECATED_SHARED_WORKER = 68;
}

// Option 2: Remove entry and explicitly reserve tag number and name:
message PageLoadMetrics {
  optional uint64 navigation_id = 1;

  reserved 2;
  reserved "legacy_event_count";
}

// When defining or re-introducing a field, remove it from reserved list:
message ChromeTrackEvent {
  reserved 1045;

  optional AccessibilityEvent accessibility_event = 1080;
}
```
