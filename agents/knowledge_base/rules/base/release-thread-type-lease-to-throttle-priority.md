---
name: base/release-thread-type-lease-to-throttle-priority
rubric: correctness
severity: should_fix
---

**Rule.** Do not rely on PlatformThread::SetCurrentThreadType() to throttle or
demote thread priority when PlatformThread::RaiseThreadTypeLease may be active
on that thread. Components requiring priority reduction must coordinate with
lease holders to release or suppress active leases.

**Why.** PlatformThread::RaiseThreadTypeLease calculates effective thread
priority by taking std::max across all active leases and the base thread type.
Calling SetCurrentThreadType() only updates the base priority and cannot lower
the effective priority below any active lease.

## Examples

Bad:

```cpp
// If MainThreadSchedulerImpl holds RaiseThreadTypeLease(kPresentation),
// SetCurrentThreadType only updates the base priority; the effective
// priority remains kPresentation because leases take precedence via std::max.
if (feature == SchedulingPolicy::Feature::kWebRTC) {
  base::PlatformThread::SetCurrentThreadType(base::ThreadType::kDefault);
}
```

Good:

```cpp
// Coordinate with the component holding the lease to drop or suppress it
// when throttling or lower priority is needed:
if (feature == SchedulingPolicy::Feature::kWebRTC) {
  if (thread_type_throttled_to_default_count_++ == 0) {
    main_thread_scheduler_->IncreaseDefaultThreadTypeUsageCount();
  }
}

// In MainThreadSchedulerImpl:
if (default_thread_type_usage_count_ > 0) {
  raise_thread_type_lease_.reset();
}
```
