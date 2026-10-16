---
name: general/document-probabilistic-counts-as-upper-bounds
rubric: readability
severity: optional
---

**Rule.** When documenting parameters, template arguments, or operations in
probabilistic, hash-based, or deduplicating structures (such as Bloom filters)
where hash collisions or overlapping indices can reduce actual items set,
describe counts as upper bounds (e.g. 'sets up to N bits') rather than exact
counts. Add an explanatory comment clarifying that fewer distinct items, slots,
or bits may be populated due to collisions.

**Why.** Describing a count as an exact value in probabilistic or hash-based
data structures is inaccurate and misleading. Independent hash functions or key
hashes can map to identical indices, so explicitly documenting the value as an
upper bound clarifies expected behavior and prevents confusion over whether
collision reduction is intentional.

## Examples

Bad:

```cpp
// A bloom filter of `kMaxLockFreeBloomFilterBits` size that sets `BitsPerKey`
// bits per entry.
template <size_t BitsPerKey>
class LockFreeBloomFilter;
```

Good:

```cpp
// A bloom filter of `kMaxLockFreeBloomFilterBits` size that sets up to
// `BitsPerKey` bits per entry. (Fewer bits can be set for a given entry if
// multiple hash functions return the same index.)
template <size_t BitsPerKey>
class LockFreeBloomFilter;
```
