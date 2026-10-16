---
name: general/avoid-umahistogrammemorykb-for-sub-megabyte-sizes
rubric: correctness
severity: should_fix
---

**Rule.** Do not use base::UmaHistogramTimes (or UMA_HISTOGRAM_TIMES) for
operations expected to complete in under 1 millisecond, and do not use
base::UmaHistogramMemoryKB (or UMA_HISTOGRAM_MEMORY_KB) for memory, buffer, or
payload sizes expected to fall below 1000 KiB (~1 MiB). Instead, use microsecond
timing helpers (base::UmaHistogramMicrosecondsTimes or
base::UmaHistogramCustomMicrosecondsTimes) for sub-millisecond durations, and
count histograms (base::UmaHistogramCounts10M or base::UmaHistogramCustomCounts)
for sub-1000 KiB sizes.

**Why.** base::UmaHistogramTimes and base::UmaHistogramMemoryKB have fixed
minimum bucket boundaries of 1 ms and 1000 KiB, respectively. Any sample falling
below these thresholds collapses into bucket 0 (underflow), discarding all
measurement granularity. Using microsecond timing helpers or count histograms
ensures proper bucket resolution across sub-millisecond or sub-megabyte
distributions.

## Examples

Bad:

```cpp
// 1. Sub-millisecond duration collapses into bucket 0 (1 ms minimum bucket):
base::UmaHistogramTimes("Android.LibraryLoader.Prefetch.ForkDuration",
                        base::TimeTicks::Now() - fork_start_time);

// 2. Serialization size under 1000 KiB collapses into bucket 0:
base::UmaHistogramMemoryKB("ImportantFile.SerializationSize",
                           static_cast<int>(previous_data_size_ / 1024));
```

Good:

```cpp
// 1. Use microsecond histogram helper to preserve sub-millisecond resolution:
base::UmaHistogramCustomMicrosecondsTimes(
    "Android.LibraryLoader.Prefetch.ForkDuration",
    base::TimeTicks::Now() - fork_start_time, base::Microseconds(1),
    base::Seconds(1), 50);

// 2. Use count histogram to record buffer sizes under 1000 KiB:
base::UmaHistogramCounts10M("ImportantFile.SerializationSize",
                            static_cast<int>(previous_data_size_));
```
