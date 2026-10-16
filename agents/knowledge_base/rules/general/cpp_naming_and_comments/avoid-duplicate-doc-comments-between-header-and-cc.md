---
name: general/avoid-duplicate-doc-comments-between-header-and-cc
rubric: readability
severity: nit
---

**Rule.** Do not duplicate doc comments between header (`.h`) and implementation
(`.cc`/`.mm`) files. Document public declarations, feature flags
(`BASE_DECLARE_FEATURE`), constants, and symbols once in the header file as the
single source of truth, reserving implementation comments for explaining
non-obvious internal mechanics.

**Why.** Duplicating comments across header and implementation files creates
maintenance overhead and causes documentation to drift out of sync when one copy
is updated and the other is missed. Centralizing usage documentation in the
header keeps interface contracts clear and authoritative.

## Examples

Bad:

```cpp
// In memory_coordinator_features.h:
// When enabled, MemoryPressureListeners change cache sizes during memory pressure.
BASE_EXPORT BASE_DECLARE_FEATURE(kStatefulMemoryPressure);

// In memory_coordinator_features.cc:
// When enabled, MemoryPressureListeners change cache sizes during memory pressure.
BASE_FEATURE(kStatefulMemoryPressure, base::FEATURE_DISABLED_BY_DEFAULT);
```

Good:

```cpp
// In memory_coordinator_features.h:
// When enabled, MemoryPressureListeners change cache sizes during memory pressure.
BASE_EXPORT BASE_DECLARE_FEATURE(kStatefulMemoryPressure);

// In memory_coordinator_features.cc:
BASE_FEATURE(kStatefulMemoryPressure, base::FEATURE_DISABLED_BY_DEFAULT);
```
