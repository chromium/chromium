---
name: general/avoid-reserving-worst-case-capacity
rubric: safety
severity: should_fix
---

**Rule.** Do not call .reserve() on std::string, std::vector, or dynamic
containers with a worst-case maximum upper bound (such as crash key limits or
maximum buffer bounds) when typical payload sizes are much smaller. Only reserve
capacity when the expected size is known or typically large enough to warrant
pre-allocation.

**Why.** Reserving container capacity up to an upper bound immediately forces a
dynamic heap allocation and wastes memory when typical payloads use only a small
fraction of the reserved capacity. For std::string, reserving unnecessary
capacity bypasses Small String Optimization (SSO), turning what would otherwise
be a zero-allocation inline string into a heap-allocated buffer.

## Examples

Bad:

```cpp
std::string list_of_hung_thread_ids;
// BAD: Pre-allocating worst-case crash key size forces an immediate heap allocation,
// even when typical thread lists fit within Small String Optimization (SSO) storage.
list_of_hung_thread_ids.reserve(
    static_cast<size_t>(debug::CrashKeySize::Size256));
```

Good:

```cpp
std::string list_of_hung_thread_ids;
// GOOD: Omit reserve() when typical usage fits within SSO, or reserve
// based only on known/expected element counts rather than worst-case bounds.
```
