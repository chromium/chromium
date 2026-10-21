---
name: base/handle-terminal-deallocation-early-in-partitionalloc-pipelin
rubric: design
severity: optional
---

**Rule.** In PartitionAlloc's deallocation pipeline, handle terminal
deallocation modes (such as intentional leaks via FreeFlags::kIntendedLeak)
early at the entry of the free chain (FreeNoHooksImmediateInternal) rather than
deep inside lower-level freeing helpers (such as FreeInSlotSpan or
SlotSpanMetadata::Free). Verify that terminal modes short-circuit deallocation
before entering thread caching, quarantines, or lock-protected metadata updates.

**Why.** Permanent leaking is a strictly stronger guarantee than intermediate
quarantining (such as BRP or SchedulerLoopQuarantine). Handling terminal
deallocations at the entry point avoids unnecessary thread cache routing,
intermediate quarantining, and lock acquisition, preventing invalid intermediate
states in lower-level freeing machinery.

## Examples

Bad:

```cpp
PA_ALWAYS_INLINE void PartitionRoot::FreeInSlotSpan(
    internal::UntaggedSlotStart slot_start,
    SlotSpanMetadata* slot_span) {
  // Leaking deep in the slot-span handler after quarantine checks.
  if (settings_.enable_intended_leak) [[unlikely]] {
    intended_leak_size_.fetch_add(slot_span->bucket->slot_size,
                                  std::memory_order_relaxed);
    return;
  }
  return slot_span->Free(slot_start.value(), this);
}
```

Good:

```cpp
template <FreeFlags flags>
PA_ALWAYS_INLINE void PartitionRoot::FreeNoHooksImmediateInternal(
    uintptr_t address,
    SlotSpanMetadata* slot_span,
    PartitionRoot::SizeDetails size_details) {
  if constexpr (ContainsFlags(flags, FreeFlags::kIntendedLeak)) {
    PA_CHECK(!settings_.with_thread_cache);
#if PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
    PA_CHECK(!brp_enabled());
#endif
    intended_leak_size_.fetch_add(size_details.slot_size);
    return;  // Early return bypasses quarantines, caches, and locks.
  }
  ...
}
```
