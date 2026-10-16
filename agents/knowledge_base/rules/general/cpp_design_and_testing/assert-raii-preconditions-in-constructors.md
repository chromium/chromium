---
name: general/assert-raii-preconditions-in-constructors
rubric: design
severity: should_fix
---

**Rule.** Assert preconditions and mutual exclusion invariants for scoped RAII
objects, leases, or guards directly at their entry points (such as constructors
or factory functions) rather than deferring compound checks into internal state
recalculation or update helpers.

**Why.** Internal recalculation and update helpers are invoked across multiple
lifecycle paths (such as dropping leases, resetting state, or applying defaults)
and should not be burdened with compound scope invariants. Asserting
preconditions at the point of acquisition catches contract violations
immediately and keeps downstream update logic simple.

## Examples

Bad:

```cpp
void ThreadTypeManager::MaybeUpdate() {
  auto highest_lease = raise_leases_.GetHighestLease();
  // Anti-pattern: Compound mutual-exclusion precondition checked inside
  // downstream update helper invoked from multiple state transitions.
  DCHECK(!(ScopedBoostablePriority::CurrentThreadHasScope() &&
           highest_lease.has_value()));
  // ...
}
```

Good:

```cpp
PlatformThreadBase::RaiseThreadTypeLease::RaiseThreadTypeLease(
    ThreadType thread_type,
    internal::ThreadTypeManager* manager)
    : leased_thread_type_(thread_type), manager_(manager) {
  // Precondition asserted directly at RAII scope entry point.
  DCHECK(!ScopedBoostablePriority::CurrentThreadHasScope());
  manager_->AcquireRaiseLease(thread_type);
}
```
