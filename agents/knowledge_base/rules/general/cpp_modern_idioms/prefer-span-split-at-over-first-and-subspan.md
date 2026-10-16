---
name: general/prefer-span-split-at-over-first-and-subspan
rubric: readability
severity: should_fix
---

**Rule.** When partitioning a base::span into two contiguous, non-overlapping
parts at an offset (e.g. an aligned prefix and remainder), use
span.split_at(offset) or span.split_at<N>() with structured binding instead of
separately calling span.first(offset) and span.subspan(offset).

**Why.** base::span::split_at partitions a span into a pair of contiguous spans
in a single operation. This avoids duplicating offset calculations and
eliminates the risk of boundary mismatches or off-by-one errors between separate
.first() and .subspan() calls.

## Examples

Bad:

```cpp
size_t aligned_bytes = (dst.size() / 4) * 4;
base::span<uint32_t> dst_u32 =
    base::subtle::reinterpret_span<uint32_t>(dst.first(aligned_bytes));
for (uint32_t& val : dst_u32) {
  val = rng();
}
for (uint8_t& val : dst.subspan(aligned_bytes)) {
  // ...
}
```

Good:

```cpp
const size_t aligned_bytes = (dst.size() / 4) * 4;
auto [aligned_dst, trailing_dst] = dst.split_at(aligned_bytes);
for (uint32_t& val :
     base::subtle::reinterpret_span<uint32_t>(aligned_dst)) {
  val = rng();
}
for (uint8_t& val : trailing_dst) {
  // ...
}
```
