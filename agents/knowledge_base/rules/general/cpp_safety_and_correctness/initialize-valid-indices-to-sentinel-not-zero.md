---
name: general/initialize-valid-indices-to-sentinel-not-zero
rubric: correctness
severity: nit
---

**Rule.** Do not initialize index, slot, or identifier data members to 0 by
default when 0 represents a valid element or configured entry. Initialize them
to an explicit invalid sentinel constant (such as `kInvalidIndex` or
`static_cast<size_t>(-1)`) or use `std::optional` initialized to `std::nullopt`,
ensuring unconfigured states are distinct from the zeroth element and reliably
fail bounds checks.

**Why.** Defaulting a valid 0-based index or identifier to 0 makes unconfigured
or uninitialized instances indistinguishable from the zeroth entry. Callers that
omit configuration silently alias element 0 and bypass bounds checks like
`CHECK(index < kMaxIndex)`.

## Examples

Bad:

```cpp
struct PartitionOptions {
  EnableToggle thread_cache = kDisabled;
  size_t thread_cache_index = 0;  // Bug: 0 is a valid index, aliasing slot 0 when unset!
};
```

Good:

```cpp
inline constexpr size_t kInvalidThreadCacheIndex = static_cast<size_t>(-1);

struct PartitionOptions {
  EnableToggle thread_cache = kDisabled;
  size_t thread_cache_index = kInvalidThreadCacheIndex;  // Fails unsigned bounds checks if unset.
};
```
