---
name: general/audit-callers-and-remove-obsolete-metrics-on-query-semantic
rubric: correctness
severity: should_fix
---

**Rule.** When changing the calculation or return semantics of a query API (such
as system info or memory queries), audit all downstream callers and metrics that
previously adjusted for the old behavior. Update or remove call-site workarounds
that would double-count values, and cleanly delete obsolete experimental metrics
along with their entries in histograms.xml.

**Why.** Downstream callers often incorporate manual adjustments or experimental
telemetry to compensate for prior API limitations. When the core API is updated
to natively include those components, unadjusted callers will double-count
values and record corrupted telemetry or execute incorrect logic.

## Examples

Bad:

```cpp
// base/system/sys_info_mac.mm
ByteSize SysInfo::AmountOfAvailablePhysicalMemoryImpl() {
  SystemMemoryInfo info;
  if (!GetSystemMemoryInfo(&info)) {
    return ByteSize(0);
  }
  return info.GetAvailablePhysicalMemory();  // Now natively includes file_backed memory!
}

// chrome/browser/performance_manager/metrics/metrics_provider_common.cc
// Unadjusted downstream caller continues adding file_backed, double-counting it:
base::UmaHistogramPercentage(
    "Memory.Experimental.MacAvailableMemoryPercentFreePageCache2",
    (available_bytes + info.file_backed).InBytes() * 100u /
        total_bytes.InBytes());
```

Good:

```cpp
// chrome/browser/performance_manager/metrics/metrics_provider_common.cc
// Downstream caller is updated and the obsolete workaround metric recording is deleted:
// (Removed obsolete UmaHistogramPercentage call for Memory.Experimental.MacAvailableMemoryPercentFreePageCache2)

// tools/metrics/histograms/metadata/memory/histograms.xml
// Also delete the obsolete metric definition from histograms.xml:
// (Removed <histogram name="Memory.Experimental.MacAvailableMemoryPercentFreePageCache2" ...>)
```
