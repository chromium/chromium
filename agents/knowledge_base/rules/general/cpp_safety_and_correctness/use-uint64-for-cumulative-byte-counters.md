---
name: general/use-uint64-for-cumulative-byte-counters
rubric: correctness
severity: nit
---

**Rule.** Use uint64_t (or std::atomic\<uint64_t>) rather than size_t for
cumulative memory statistics, lifetime allocation counters, and monotonic byte
totals. Do not use size_t for metrics that accumulate across a process's
lifetime, as they can exceed 4 GiB and overflow on 32-bit architectures where
size_t is 32 bits.

**Why.** While single-buffer sizes or instantaneous allocations cannot exceed
the 4 GiB address space on 32-bit platforms, cumulative metrics (such as memory
churn, aggregate allocations, total leaked bytes, or cache throughput)
accumulate monotonically over time and realistically exceed 4 GiB. Because
size_t is only 32 bits on 32-bit architectures, using size_t or
std::atomic\<size_t> for lifetime cumulative totals causes silent integer
wrap-around and corrupts telemetry and diagnostics.

## Examples

Bad:

```cpp
// Bad: size_t is 32-bit on 32-bit systems and overflows after 4 GiB of cumulative bytes.
struct PartitionMemoryStats {
  size_t total_intended_leak_bytes;
  size_t total_allocated_bytes;
};

std::atomic<size_t> intended_leak_size_{0};
size_t total_bytes_allocated_{0};
```

Good:

```cpp
// Good: uint64_t prevents 32-bit overflow across long-running process lifetimes.
struct PartitionMemoryStats {
  uint64_t total_intended_leak_bytes;
  uint64_t total_allocated_bytes;
};

std::atomic<uint64_t> intended_leak_size_{0};
uint64_t total_bytes_allocated_{0};
```
