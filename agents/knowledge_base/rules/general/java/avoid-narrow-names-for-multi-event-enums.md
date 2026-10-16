---
name: general/avoid-narrow-names-for-multi-event-enums
rubric: readability
severity: should_fix
---

**Rule.** Do not name an enum enumerator, category, or task scope after a single
specific event or subcase when it encompasses multiple distinct event types
(such as both load and error events). Use an appropriately general umbrella
identifier, especially when companion trace slices, arguments, or payload fields
already record fine-grained event details.

**Why.** Naming an enum or category after a single specific event misleads
readers into assuming it only applies to that event, obscuring shared handling
logic and telemetry across sibling events. Furthermore, over-specifying event
details in high-level scope enums creates unnecessary granularity when companion
trace slices or structured payload fields already capture the exact event type.

## Examples

Bad:

```cpp
// Misleadingly named for only one sub-event when used for load and error events:
enum TaskScopeType {
  TASK_SCOPE_UNSPECIFIED = 0;
  TASK_SCOPE_LOAD_EVENT = 1;
};
```

Good:

```cpp
// Broadly named to encompass load, error, and related event types:
enum TaskScopeType {
  TASK_SCOPE_UNSPECIFIED = 0;
  TASK_SCOPE_MISC_EVENT = 1;
};
```
