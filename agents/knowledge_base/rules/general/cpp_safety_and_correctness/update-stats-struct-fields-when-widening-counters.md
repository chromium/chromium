---
name: general/update-stats-struct-fields-when-widening-counters
rubric: correctness
severity: nit
---

**Rule.** When widening the integer type of an internal counter, accumulator, or
metric (such as from size_t or uint32_t to uint64_t), update all corresponding
fields in data transfer, stats, and telemetry reporting structs to match. Do not
leave counterpart reporting struct fields typed as platform-dependent or
narrower types.

**Why.** Leaving reporting or stats struct fields as narrower or
platform-dependent types (like size_t on 32-bit platforms) silently truncates
upper bits when transferring metric values, defeating the counter widening and
corrupting telemetry.

## Examples

Bad:

```cpp
// In internal state header (e.g. partition_root.h):
std::atomic<uint64_t> intended_leak_size_;

// In public stats struct (e.g. partition_stats.h):
struct PartitionMemoryStats {
  size_t total_intended_leak_bytes;  // Truncates to 32 bits on 32-bit platforms!
};
```

Good:

```cpp
// In internal state header (e.g. partition_root.h):
std::atomic<uint64_t> intended_leak_size_;

// In public stats struct (e.g. partition_stats.h):
struct PartitionMemoryStats {
  uint64_t total_intended_leak_bytes;
};
```
