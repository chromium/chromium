---
name: general/when-separating-public-and-internal-interfaces-reserve-canon
rubric: readability
severity: should_fix
---

**Rule.** When organizing header files, reserve canonical header names
(`<name>.h`) for public APIs and use `_internal.h`, `_impl.h`, or `_subtle.h`
suffixes (or co-locate with existing internal/subtle headers) for low-level
mechanics and utilities in `internal` or `subtle` namespaces. Do not name public
interfaces `<name>_external.h`, and do not expose internal or `subtle` helpers
under general-purpose, top-level public header names or clutter public API
headers with internal macro and template mechanics.

**Why.** Canonical header names signal a general-purpose public API meant for
widespread use. Exposing internal or subtle utilities under generic header names
or inverting naming with `_external.h` misleads callers about visibility,
clutters public interfaces, and obscures which headers consumers should include.

## Examples

Bad:

```cpp
// 1. Inverting canonical and internal naming:
// In bounds_checks.h (internal implementation for base::span):
bool IsExtentOutOfBounds(const void* ptr, size_t extent_bytes, size_t type_size);
// In bounds_checks_external.h (public API for general callers):
template <typename T>
bool IsExtentInBounds(const T* ptr, StrictNumeric<size_t> index);

// 2. Cluttering public header with internal macro/template mechanics:
// In base/feature.h:
#define BASE_FEATURE_INTERNAL_3_ARGS(feature, name, default_state) ...
namespace base::internal {
enum class FeatureMacroHandshake { kSecret };
}  // namespace base::internal

// 3. Exposing subtle/internal namespace under a generic public header:
// In base/yield_processor.h:
namespace base::subtle {
inline void YieldProcessor() { ... }
}  // namespace base::subtle
```

Good:

```cpp
// 1. Public API in canonical header, internals in _internal.h:
// In bounds_checks.h (public API for general callers):
template <typename T>
bool IsExtentInBounds(const T* ptr, StrictNumeric<size_t> index);
// In bounds_checks_internal.h (internal implementation):
bool IsExtentOutOfBounds(const void* ptr, size_t extent_bytes, size_t type_size);

// 2. Extract macro/template mechanics into companion _internal.h:
// In base/feature_internal.h:
#define BASE_FEATURE_INTERNAL_3_ARGS(feature, name, default_state) ...
// In base/feature.h:
#include "base/feature_internal.h"

// 3. Reflect subtle/internal namespace in header name or co-locate:
// In base/synchronization/lock_subtle.h:
namespace base::subtle {
inline void YieldProcessor() { ... }
}  // namespace base::subtle
```
