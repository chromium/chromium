---
name: general/avoid-magic-numbers-use-named-constants
rubric: readability
severity: should_fix
---

**Rule.** Do not use raw numeric literals or compounded magic offsets for buffer
capacities, array indexing, slot tables, sentinel values, threshold checks,
assertions, or test slot allocations. Define or reference explicit,
self-documenting compile-time constants (such as constexpr or inline constexpr
k... constants), derive indices from base definitions when allocating slots in
tests, and express boundary calculations using named semantic intermediate
variables. When calculations or assertions account for implicit sentinel entries
(such as histogram overflow buckets or 0-indexing adjustments), document the
extra slots in comments rather than compounding unexplained offsets, and enforce
bounds using static_assert.

**Why.** Hardcoded numeric literals and compound magic offsets obscure semantic
meaning and create fragile code that looks like an arithmetic bug or breaks when
buffer sizes, layouts, capacities, threshold bounds, sentinel parameters, or
default values change. Expressing calculations through semantic intermediate
constants documents provenance, while inline comments clarify implicit sentinel
slots and static_assert verifies bounds invariants at compile time.

## Examples

Bad:

```cpp
// Bad: Hardcoded buffer sizes, magic offsets, raw slot indices, and unverified bounds.
BufferedDwarfReader<4096> reader(fd, offset);
ThreadCache* tcache = ThreadCache::Get(0);
free_func(root, allocations[i], sizes[i], 0);
PA_CHECK(opts.thread_cache_index >= 2);

// Compounding unexplained offsets for 0-indexing and implicit overflow buckets:
constexpr uintmax_t kLinearHistogramBucketCountMax =
    static_cast<uintmax_t>(LinearHistogram::kBucketCount_MAX) - 2;
static_assert(kMaxValue <= kLinearHistogramBucketCountMax);

// In unit tests: hardcoded slot indices without explaining origin or asserting bounds.
ASSERT_FALSE(ThreadCache::IsValid(ThreadCache::Get(2)));
auto root1 = CreateCustomTestRootWithThreadCache(options, {}, 2);
```

Good:

```cpp
// Good: Named constexpr constants clarify intent, document sentinels, and assert bounds.
BufferedDwarfReader<google::kBigFileCacheSize> reader(fd, offset);
inline constexpr size_t kThreadCacheQuarantineIndex = 0;
ThreadCache* tcache = ThreadCache::Get(kThreadCacheQuarantineIndex);
constexpr size_t kNoAlignment = 0;
free_func(root, allocations[i], sizes[i], kNoAlignment);

inline constexpr size_t kNumDefaultPartitions = 2;
PA_CHECK(opts.thread_cache_index >= kNumDefaultPartitions);

// Semantic intermediate variables and comments documenting implicit overflow slots:
constexpr auto kBucketCount = static_cast<int>(T::kMaxValue) + 1;
constexpr auto kBucketCountMax =
    static_cast<int>(LinearHistogram::kBucketCount_MAX);
// Note: UmaHistogramExactLinear() adds 1 to bucket count for the overflow bucket.
static_assert(kBucketCount < kBucketCountMax);

// In unit tests: derive indices from base constants and verify capacity bounds at compile time.
constexpr size_t index1 = kNumDefaultPartitions;
constexpr size_t index2 = kNumDefaultPartitions + 1;
static_assert(index1 < internal::kMaxThreadCacheIndex);
static_assert(index2 < internal::kMaxThreadCacheIndex);
ASSERT_FALSE(ThreadCache::IsValid(ThreadCache::Get(index1)));
auto root1 = CreateCustomTestRootWithThreadCache(options, {}, index1);
```
