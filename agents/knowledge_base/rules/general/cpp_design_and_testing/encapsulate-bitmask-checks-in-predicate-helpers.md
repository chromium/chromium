---
name: general/encapsulate-bitmask-checks-in-predicate-helpers
rubric: design
severity: nit
---

**Rule.** Encapsulate low-level bit flag and bitmask inspections in a dedicated
predicate helper function (or member method) when the check is evaluated across
multiple call sites, rather than repeating raw bitwise expressions (such as
`(val & kFlag) != 0`). Pass the packed integer or state value to the helper and
invoke it consistently at all inspection points.

**Why.** Inlining raw bitwise operations across multiple functions duplicates
bit-masking logic and leaks internal bit-packing and flag representations across
callers. Encapsulating the check behind a named predicate hides internal bit
layout details and ensures future changes to bit allocations or flag logic
remain localized.

## Examples

Bad:

```cpp
// Bad: Multiple call sites duplicate raw bitwise operations to check a flag
if ((current_cache_value & kIsRuntimeMutableFlag) != 0) {
  return GetOverrideStateByFeatureName(feature.name);
}

bool Feature::IsRuntimeMutable() const {
  return (cached_value.load(std::memory_order_relaxed) &
          kIsRuntimeMutableFlag) != 0;
}
```

Good:

```cpp
// Good: Encapsulate the bit check in a dedicated helper function
inline constexpr bool IsRuntimeMutable(uint32_t feature_cached_value) {
  return (feature_cached_value & kRuntimeMutabilityFlag) != 0;
}

if (internal::IsRuntimeMutable(current_cache_value)) {
  return GetOverrideStateByFeatureName(feature.name);
}

bool Feature::IsRuntimeMutable() const {
  return internal::IsRuntimeMutable(
      cached_value.load(std::memory_order_relaxed));
}
```
