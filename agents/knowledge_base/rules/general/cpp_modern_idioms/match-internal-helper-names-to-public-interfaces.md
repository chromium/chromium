---
name: general/match-internal-helper-names-to-public-interfaces
rubric: readability
severity: nit
---

**Rule.** Maintain consistent naming and terminology across public interfaces,
configuration options, internal settings, state counters, telemetry metrics, and
helper predicates for a feature. Do not introduce divergent synonyms or
alternative names (such as mixing no_dealloc and intended_leak) across
configuration structs, internal state, and public macros.

**Why.** Inconsistent terminology across configuration options, internal
state/counters, helper predicates, and public interfaces creates cognitive
overhead, impairs codebase searchability, and obscures the connection between
configuration settings and runtime behavior.

## Examples

Bad:

```cpp
// Bad: Helper uses 'LeakySecurityObject' while the public macro is LEAKED_SANITIZED_OBJECT.
#define LEAKED_SANITIZED_OBJECT(...) ...
constexpr bool IsLeakySecurityObject(MemorySafetyCheck checks) {
  return checks & MemorySafetyCheck::kLeakedSanitizedObject;
}

// Bad: Option uses 'no_dealloc' while state and metrics use 'intended_leak'.
struct PartitionOptions {
  EnableToggle no_dealloc = kDisabled;
};
if (settings_.enable_no_dealloc) {
  intended_leak_size_.fetch_add(size, std::memory_order_relaxed);
}
```

Good:

```cpp
// Good: Predicate name mirrors the public macro and enum terminology.
#define LEAKED_SANITIZED_OBJECT(...) ...
constexpr bool IsLeakedSanitizedObject(MemorySafetyCheck checks) {
  return checks & MemorySafetyCheck::kLeakedSanitizedObject;
}

// Good: Config option, setting, and state counter use uniform 'intended_leak' naming.
struct PartitionOptions {
  EnableToggle intended_leak = kDisabled;
};
if (settings_.enable_intended_leak) {
  intended_leak_size_.fetch_add(size, std::memory_order_relaxed);
}
```
