---
name: general/use-unchecked-tag-for-known-valid-span-construction
rubric: safety
severity: should_fix
---

**Rule.** When constructing a base::span from a known-valid C++ reference or
provably safe object/buffer (such as inside single-object span conversion
helpers or audited buffer adapters), pass the base::unchecked tag to the
base::span constructor to elide redundant runtime heap bounds checks under
PA_BUILDFLAG(CHECKED_SPAN).

**Why.** Under PA_BUILDFLAG(CHECKED_SPAN), the standard base::span constructor
queries PartitionAlloc to verify pointer extents at runtime. When constructing a
span from an already-validated C++ reference or provably in-bounds internal
storage, runtime PartitionAlloc bounds queries add unnecessary overhead on
performance-sensitive paths.

## Examples

Bad:

```cpp
template <typename T>
constexpr auto span_from_ref(T& t LIFETIME_BOUND) {
  // Bypasses unsafe buffers but still performs PartitionAlloc extent checks.
  return UNSAFE_BUFFERS(span<T, 1>(std::addressof(t), 1u));
}
```

Good:

```cpp
template <typename T>
constexpr auto span_from_ref(T& t LIFETIME_BOUND) {
  // Uses base::unchecked to elide redundant PartitionAlloc checks for guaranteed valid references.
  return UNSAFE_BUFFERS(span<T, 1>(unchecked, std::addressof(t), 1u));
}
```
