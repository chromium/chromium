---
name: general/never-trust-external-shared-memory-hashes-or-metadata
rubric: safety
severity: nit
---

**Rule.** Do not trust precomputed hashes, cached validation flags, or indexing
metadata received from external processes or shared memory (such as
PersistentHistogramAllocator or shared memory buffers). Always recompute hashes
and validate bounds and identifiers locally in the consuming or privileged
process.

**Why.** In multi-process architectures, shared memory or IPC structures can be
manipulated by compromised or untrusted child processes. Relying on external
precomputed hashes or validation metadata bypasses integrity checks, enabling
hash spoofing, collision exploits, or accessing incorrect objects in shared
registries.

## Examples

Bad:

```cpp
// BAD: Trusting precomputed name_hash() from an external persistent histogram allocator
HistogramBase* existing = StatisticsRecorder::FindHistogram(
    histogram->name_hash(), lookup_name);
```

Good:

```cpp
// GOOD: Recomputing hash locally because the external object's name_hash() is untrusted
HistogramBase* existing = StatisticsRecorder::FindHistogram(lookup_name);
```
