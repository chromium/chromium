---
name: base/avoid-double-counting-in-kernel-memory-counters
rubric: design
severity: should_fix
---

**Rule.** When calculating available or aggregate memory metrics from OS kernel
counters (such as Mach vm_statistics64 or Linux /proc/meminfo), verify the
kernel's accounting semantics before summing fields, and never sum subcategories
if the kernel or parent counter already includes them. Additionally, when
implementing platform memory queries in SysInfo (such as
AmountOfAvailablePhysicalMemoryImpl), delegate directly to
SystemMemoryInfo::GetAvailablePhysicalMemory() rather than manually re-computing
or summing raw memory counters.

**Why.** Platform memory accounting counters often represent overlapping or
nested categories (for example, Mach free_count includes speculative_count),
leading to double-counting and inflated availability estimates. Re-implementing
ad-hoc calculations in SysInfo instead of delegating to
SystemMemoryInfo::GetAvailablePhysicalMemory introduces maintenance overhead and
behavioral divergence across base memory query APIs.

## Examples

Bad:

```cpp
// Bad: re-computing available memory manually and double-counting speculative pages
ByteSize SysInfo::AmountOfAvailablePhysicalMemoryImpl() {
  SystemMemoryInfo info;
  if (!GetSystemMemoryInfo(&info)) {
    return ByteSize(0);
  }
  return info.free + info.speculative + info.file_backed;
}
```

Good:

```cpp
// Good: delegate directly to canonical SystemMemoryInfo helper
ByteSize SysInfo::AmountOfAvailablePhysicalMemoryImpl() {
  SystemMemoryInfo info;
  if (!GetSystemMemoryInfo(&info)) {
    return ByteSize(0);
  }
  return info.GetAvailablePhysicalMemory();
}
```
