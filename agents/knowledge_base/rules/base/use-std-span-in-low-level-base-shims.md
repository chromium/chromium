---
name: base/use-std-span-in-low-level-base-shims
rubric: design
severity: blocking
---

**Rule.** In headers of low-level helper subtargets or shims that `//base`
depends on (such as `base/simdutf_shim.h`), use standard library `std::span`
(from `<span>`) instead of `base::span` from `base/containers/span.h`. Callers
in `//base` can pass `base::span` directly to these interfaces because
`base::span` implicitly converts to `std::span`.

**Why.** Including `base/containers/span.h` in low-level helper targets that
`//base` depends on creates a circular include dependency between `//base` and
the helper target. Using `std::span` eliminates circular dependencies and avoids
`allow_circular_includes_from` while preserving type-safe span parameters.

## Examples

Bad:

```cpp
// base/simdutf_shim.h
#include "base/containers/span.h"

namespace base::internal {
// BAD: Causes a circular GN/header dependency because //base depends on this shim.
[[nodiscard]] size_t simdutf_binary_to_base64(span<const uint8_t> input,
                                              span<char> binary_output);
}
```

Good:

```cpp
// base/simdutf_shim.h
#include <span>

namespace base::internal {
// Deliberately uses std::span to avoid having a circular dependency on //base.
[[nodiscard]] size_t simdutf_binary_to_base64(std::span<const uint8_t> input,
                                              std::span<char> binary_output);
}
```
