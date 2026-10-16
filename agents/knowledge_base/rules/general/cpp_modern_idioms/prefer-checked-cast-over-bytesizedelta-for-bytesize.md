---
name: general/prefer-checked-cast-over-bytesizedelta-for-bytesize
rubric: readability
severity: should_fix
---

**Rule.** Do not use `base::ByteSizeDelta` or signed unit helpers (`KiBS`,
`MiBS`, `GiBS`, etc.) combined with `.AsByteSize()` to construct an absolute
`base::ByteSize` from runtime signed integers; prefer unsigned types at the
source or explicitly convert with `base::checked_cast<uint64_t>()` (or
`base::checked_cast<unsigned>()`) passed to `ByteSize` or unsigned unit helpers
(`KiB`, `MiB`, `GiB`). Additionally, do not add redundant `CHECK(val >= 0)`
assertions immediately before constructing `base::ByteSize`, as `ByteSize`
inherently enforces the `[0...max(int64_t)]` range and fails fast upon
construction on negative or out-of-range values.

**Why.** `base::ByteSizeDelta` represents signed size differences, whereas
`base::ByteSize` represents non-negative absolute quantities in
`[0...max(int64_t)]`. Using `ByteSizeDelta` solely as a type conversion
workaround degrades semantic type safety, while preceding `ByteSize`
construction with manual non-negativity `CHECK` assertions is redundant because
`base::ByteSize` and `base::checked_cast` already validate non-negativity and
bounds.

## Examples

Bad:

```cpp
// Bad: Using ByteSizeDelta or signed unit helpers with .AsByteSize().
long page_size = sysconf(_SC_PAGESIZE);
return base::ByteSizeDelta(page_size).AsByteSize() * pages;

// Bad: Adding redundant CHECKs before ByteSize construction.
CHECK(available >= 0);
CHECK(total >= 0);
return DiskSpaceInfo{
    .total = ByteSize(static_cast<uint64_t>(total)),
    .available = ByteSize(static_cast<uint64_t>(available))};
```

Good:

```cpp
// Good: Using base::checked_cast or unsigned unit helpers.
long page_size = sysconf(_SC_PAGESIZE);
return base::ByteSize(base::checked_cast<uint64_t>(page_size)) * pages;

// Good: Constructing ByteSize directly without redundant CHECK assertions.
return DiskSpaceInfo{
    .total = ByteSize(base::checked_cast<uint64_t>(total)),
    .available = ByteSize(base::checked_cast<uint64_t>(available))};
```
