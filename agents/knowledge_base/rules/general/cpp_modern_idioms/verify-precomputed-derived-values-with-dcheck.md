---
name: general/verify-precomputed-derived-values-with-dcheck
rubric: readability
severity: nit
---

**Rule.** When an API or constructor accepts both a source object/identifier and
a precomputed derived value (such as a hash, checksum, or key) to avoid
redundant computation, assert with `DCHECK_EQ` that the passed precomputed value
matches the recomputed source value. Do not use `CHECK_EQ` when recomputing the
derived value would defeat the performance purpose of passing the precomputed
value in production.

**Why.** Precomputing and passing derived values optimizes release performance
but introduces the risk of callers passing stale or mismatched values. Asserting
equality within `DCHECK_EQ` validates the invariant during debug and test runs
while compiling out completely in official release builds with zero runtime
overhead.

## Examples

Bad:

```cpp
// Caller passes precomputed name_hash to avoid hashing overhead in production,
// but consistency with histogram_name() is never validated.
Histogram::Histogram(DurableStringView durable_name,
                     uint64_t name_hash,
                     const BucketRanges* ranges)
    : HistogramBase(durable_name) {
  DCHECK(ranges) << histogram_name();
  unlogged_samples_ = std::make_unique<SampleVector>(name_hash, ranges);
}
```

Good:

```cpp
// Validates that precomputed name_hash matches the source name in debug builds
// without adding hashing overhead to release builds.
Histogram::Histogram(DurableStringView durable_name,
                     uint64_t name_hash,
                     const BucketRanges* ranges)
    : HistogramBase(durable_name) {
  DCHECK(ranges) << histogram_name();
  DCHECK_EQ(name_hash, HashMetricName(histogram_name()));
  unlogged_samples_ = std::make_unique<SampleVector>(name_hash, ranges);
}
```
