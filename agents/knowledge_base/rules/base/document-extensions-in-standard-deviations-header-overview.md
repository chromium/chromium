---
name: base/document-extensions-in-standard-deviations-header-overview
rubric: readability
severity: nit
---

**Rule.** When adding new methods, operators, or convenience extensions to a
vocabulary or container type whose header maintains an enumerated overview of
deviations from a standard library counterpart (e.g., 'Differences from
std::...' or 'Differences from \[span....\]:'), update that header overview to
document the new extension, its syntax, and its rationale.

**Why.** Headers for vocabulary and container types that emulate standard C++
types provide top-level deviation overviews so developers can quickly understand
non-standard extensions and safety differences. Omitting new convenience methods
from this overview causes file-level documentation to become stale and prevents
callers from discovering enhanced functionality.

## Examples

Bad:

```cpp
// In base/containers/span.h:
// Differences from `std::span`
// Differences from [span.cons]:
// - For convenience, provides `span::copy_from[_nonoverlapping]()` as wrappers
//   around `std::ranges::copy()` that enforce equal-size spans.
// - For convenience, provides `span::copy_prefix_from()` to allow copying into
//   the beginning of the current span.
//
// Differences from [span.deduct]:

// Implementation below adds:
constexpr void copy_from_volatile(span<const volatile T, N> other);
```

Good:

```cpp
// In base/containers/span.h:
// Differences from `std::span`
// Differences from [span.cons]:
// - For convenience, provides `span::copy_from[_nonoverlapping]()` as wrappers
//   around `std::ranges::copy()` that enforce equal-size spans.
// - For convenience, provides `span::copy_prefix_from()` to allow copying into
//   the beginning of the current span.
// - For convenience, provides `span::copy_from_volatile()` as wrappers to copy
//   from volatile memory that enforce equal-size spans.
//
// Differences from [span.deduct]:
```
