---
name: base/validate-partition-thread-cache-index-bounds
rubric: correctness
severity: should_fix
---

**Rule.** When initializing a partition with thread caching configured in
PartitionAlloc, validate via PA_CHECK that opts.thread_cache_index is less than
internal::kMaxThreadCacheIndex and verify that non-malloc partitions do not
claim the default index 0 when USE_PARTITION_ALLOC_AS_MALLOC is enabled.

**Why.** Per-thread caches in PartitionAlloc use a fixed-size table bounded by
kMaxThreadCacheIndex, where index 0 is reserved for the default partition when
PartitionAlloc acts as malloc(). An out-of-bounds index causes memory corruption
from out-of-bounds array indexing, and using index 0 for another partition
collides with the global malloc cache.

## Examples

Bad:

```cpp
void PartitionAllocator::init(PartitionOptions opts) {
  partition_root_.Init(opts);
  // ...
}
```

Good:

```cpp
void PartitionAllocator::init(PartitionOptions opts) {
#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  PA_CHECK(opts.thread_cache_index != 0 ||
           opts.thread_cache == PartitionOptions::kDisabled)
      << "Cannot use a thread cache at default index when PartitionAlloc is "
         "malloc().";
#endif
  PA_CHECK(opts.thread_cache_index < internal::kMaxThreadCacheIndex)
      << "Thread cache index must be less than kMaxThreadCacheIndex";
  partition_root_.Init(opts);
  // ...
}
```
