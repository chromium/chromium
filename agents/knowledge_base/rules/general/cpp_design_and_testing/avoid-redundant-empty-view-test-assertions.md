---
name: general/avoid-redundant-empty-view-test-assertions
rubric: readability
severity: nit
---

**Rule.** In unit tests, avoid writing duplicate assertions for functionally
identical empty view parameters (such as testing both base::span<T>() and
base::span<const T>() when the function accepts base::span<const T>). Since
non-const views implicitly convert to const views, testing multiple cv-qualified
empty instances invokes the identical overload and code path without adding test
coverage.

**Why.** When a function parameter is a const view type (such as
base::span<const T>), passing an empty mutable view span<T>() and an empty const
view span<const T>() resolves to the exact same function overload and executes
identical code paths. Duplicate assertions for identical empty arguments create
unnecessary test clutter without verifying distinct behavior.

## Examples

Bad:

```cpp
// Redundant duplicate assertions for identical empty spans:
EXPECT_EQ(HexEncode(base::span<uint8_t>()), "");
EXPECT_EQ(HexEncode(base::span<const uint8_t>()), "");
```

Good:

```cpp
// Single assertion covers the empty view case:
EXPECT_EQ(HexEncode(base::span<uint8_t>()), "");
```
