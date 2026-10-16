---
name: general/document-caching-and-freshness-limitations-in-query-apis
rubric: readability
severity: should_fix
---

**Rule.** When declaring public system, hardware, or environment query APIs that
cache values statically or lazily upon first invocation, document the caching
semantics and accuracy limitations in header comments. Explicitly clarify
whether returned values reflect a one-time snapshot at first call and may not
track dynamic environment changes (such as CPU hotplug/unplug or VM
reconfiguration).

**Why.** Callers of system query APIs naturally expect functions to reflect the
live state of the machine. When an API caches values internally with static
duration to avoid repeated I/O or sandbox constraints, documenting caching
semantics and accuracy limitations prevents callers from incorrectly expecting
dynamic freshness.

## Examples

Bad:

```cpp
// Returns the maximum frequency of all the processors in Hz, if
// available. The data may not be available in virtual machines for
// instance. In this case, the returned value is empty.
static const std::vector<uint64_t>& MaxFrequencyPerProcessor();
```

Good:

```cpp
// Returns the maximum frequency of all the processors in Hz, if
// available. The data may not be available in virtual machines for
// instance. In this case, the returned value is empty.
//
// Also note that due to caching, this is not expected to be accurate in case
// of CPU unplug, which can happen, in particular in VMs. More specifically,
// the returned value is only expected to be valid at the time where this was
// first called. Subsequent calls may return a stable value, which can be an
// under/overestimate in case of CPU hotplug.
static const std::vector<uint64_t>& MaxFrequencyPerProcessor();
```
