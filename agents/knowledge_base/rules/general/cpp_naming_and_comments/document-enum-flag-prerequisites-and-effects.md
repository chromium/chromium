---
name: general/document-enum-flag-prerequisites-and-effects
rubric: readability
severity: nit
---

**Rule.** When defining or adding enum enumerators, bitmask values, or
configuration constants that carry prerequisites (such as build flags,
platforms, or subsystem requirements) or specific effects (such as underlying
flags enabled), document each entry explicitly. Do not leave new or adjacent
entries unannotated when preceding comments describe requirements or behaviors
that might ambiguously appear to apply to subsequent entries.

**Why.** Leaving enum values or bitmask flags unannotated next to commented
entries creates ambiguity about whether preceding preconditions, platform
requirements, or enabled effects apply to adjacent entries. Explicit per-entry
documentation prevents misunderstandings during code review and maintenance.

## Examples

Bad:

```cpp
enum class MemorySafetyCheck : uint32_t {
  kNone = 0,
  kForcePartitionAlloc = (1u << 0),
  // Enables |FreeFlags::kSchedulerLoopQuarantineForAdvancedMemorySafetyChecks|.
  // Requires PA-E.
  kSchedulerLoopQuarantine = (1u << 1),
  kInfiniteQuarantine = 1u << 2,  // Ambiguous: does preceding PA-E requirement apply?
};
```

Good:

```cpp
enum class MemorySafetyCheck : uint32_t {
  kNone = 0,
  kForcePartitionAlloc = (1u << 0),
  // Enables |FreeFlags::kSchedulerLoopQuarantineForAdvancedMemorySafetyChecks|.
  // Requires PA-E.
  kSchedulerLoopQuarantine = (1u << 1),
  // Enables |FreeFlags::kInfiniteQuarantine|. Requires PA-E.
  kInfiniteQuarantine = 1u << 2,
};
```
