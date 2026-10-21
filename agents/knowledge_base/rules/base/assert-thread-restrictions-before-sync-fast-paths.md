---
name: base/assert-thread-restrictions-before-sync-fast-paths
rubric: safety
severity: nit
---

**Rule.** When implementing blocking synchronization primitives, wait
operations, or process sync helpers that have early-exit fast paths (such as
checking if an event is already signaled or if a resource is already available),
invoke thread restriction assertions (such as
internal::AssertBaseSyncPrimitivesAllowed() or base::AssertBlockingAllowed())
before the early-exit check. Do not place restriction checks after the fast path
or bypass them on pre-signaled states.

**Why.** If thread restrictions are asserted only after an early return, callers
executing on threads that forbid blocking or base sync primitives will
intermittently succeed whenever the event happens to be pre-signaled. This masks
threading contract violations that will manifest as hangs, deadlocks, or UI jank
whenever the primitive is not already signaled.

## Examples

Bad:

```cpp
bool WaitableEvent::TimedWait(TimeDelta wait_delta) {
  // Anti-pattern: Fast-path check precedes thread restriction assertion.
  if (IsSignaled()) {
    return true;
  }
  internal::AssertBaseSyncPrimitivesAllowed();
  return TimedWaitImpl(wait_delta);
}
```

Good:

```cpp
bool WaitableEvent::TimedWait(TimeDelta wait_delta) {
  // Always verify thread restrictions before early exit to prevent
  // fortuitous allowance on threads that forbid sync primitives.
  internal::AssertBaseSyncPrimitivesAllowed();
  if (IsSignaled()) {
    return true;
  }
  return TimedWaitImpl(wait_delta);
}
```
