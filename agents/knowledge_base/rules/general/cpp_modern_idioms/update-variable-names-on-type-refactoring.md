---
name: general/update-variable-names-on-type-refactoring
rubric: readability
severity: should_fix
---

**Rule.** When refactoring an API, data structure, or function to replace an
underlying type, enum, or concept, or when converting raw pointers and offsets
to view types (such as base::span or std::string_view), update all associated
method, parameter, field, and variable names to reflect the new abstraction. Do
not retain stale identifier names (such as \_ptr, \_start, or obsolete enum
names) that contradict the underlying data type or semantics.

**Why.** Stale identifiers left behind after refactoring create misleading code
where names suggest one concept (such as a priority level or a single raw
pointer) while actually representing another (such as a thread type or a buffer
view). Updating names prevents confusion, eliminates misleading assumptions
about boundaries or types, and keeps code readable and maintainable.

## Examples

Bad:

```cpp
// Bad: Retaining stale concept names or pointer suffixes after type refactoring.
auto priority = priority_queue_.PeekSortKey().thread_type();
if (priority != ThreadType::kBackground) {
  ScheduleTask(priority);
}

void ProcessData(base::span<const uint8_t> data_ptr);

class EnvironmentParser {
  NativeEnvironmentStringView block_start_;
};
```

Good:

```cpp
// Good: Variable, parameter, and member names updated to match the new abstraction.
auto thread_type = priority_queue_.PeekSortKey().thread_type();
if (thread_type != ThreadType::kBackground) {
  ScheduleTask(thread_type);
}

void ProcessData(base::span<const uint8_t> data);

class EnvironmentParser {
  NativeEnvironmentStringView unparsed_view_;
};
```
