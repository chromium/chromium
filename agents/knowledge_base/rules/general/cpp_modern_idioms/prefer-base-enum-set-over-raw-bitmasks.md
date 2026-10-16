---
name: general/prefer-base-enum-set-over-raw-bitmasks
rubric: readability
severity: optional
---

**Rule.** When storing or manipulating sets of enum values, use `base::EnumSet`
(from `base/containers/enum_set.h`) instead of raw integer bitmasks and manual
bit shifts (such as `1u << static_cast<uint32_t>(enum_val)`). When deferring a
migration in existing bitmask code during a targeted change, add an attributed
`TODO` comment noting the `base::EnumSet` adoption.

**Why.** Raw integer bitmasks require manual casting from enum types, provide no
bounds checking or compile-time type safety against mismatched enums, and
require error-prone manual bitwise logic. `base::EnumSet` enforces enum ranges,
prevents accidental operations with foreign types, and provides clean member
methods like `Put()`, `Remove()`, and `Has()`.

## Examples

Bad:

```cpp
void ThreadTypeManager::RaiseLeases::Acquire(ThreadType thread_type) {
  auto type = static_cast<uint32_t>(thread_type);
  leases[type]++;
  bitmask |= (1u << type);
}
```

Good:

```cpp
// Option 1: Use base::EnumSet for type-safe enum set operations.
void ThreadTypeManager::RaiseLeases::Acquire(ThreadType thread_type) {
  leases[std::to_underlying(thread_type)]++;
  thread_types.Put(thread_type);
}

// Option 2: Add an attributed TODO if deferring refactoring during a targeted change.
void ThreadTypeManager::RaiseLeases::Acquire(ThreadType thread_type) {
  // TODO(crbug.com/470337728): Consider using base::EnumSet.
  auto type = static_cast<uint32_t>(thread_type);
  leases[type]++;
  bitmask |= (1u << type);
}
```
