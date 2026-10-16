---
name: general/check-fast-paths-before-scoped-blocking-call-will-block
rubric: safety
severity: should_fix
---

**Rule.** Before instantiating a ScopedBlockingCall (or
ScopedBlockingCallWithBaseSyncPrimitives) with BlockingType::WILL_BLOCK, check
whether the operation can be completed immediately on a non-blocking fast path
(such as checking if an event is already signaled or if cached data is
available). Return early or complete the operation before entering the blocking
scope.

**Why.** Declaring BlockingType::WILL_BLOCK informs the thread pool scheduler
that the worker thread will definitely block off-CPU, which can trigger an
immediate increase in worker thread capacity. Executing non-blocking fast paths
beforehand avoids entering a blocking scope when the result is already
available, preventing unnecessary thread creation and scheduler churn.

## Examples

Bad:

```cpp
size_t WaitableEvent::WaitMany(base::span<WaitableEvent*> events) {
  DCHECK(!events.empty()) << "Cannot wait on no events";
  internal::ScopedBlockingCallWithBaseSyncPrimitives scoped_blocking_call(
      FROM_HERE, BlockingType::WILL_BLOCK);

  const size_t signaled_id = WaitManyImpl(events);
  return signaled_id;
}
```

Good:

```cpp
size_t WaitableEvent::WaitMany(base::span<WaitableEvent*> events) {
  DCHECK(!events.empty()) << "Cannot wait on no events";

  for (size_t i = 0; i < events.size(); ++i) {
    if (events[i]->IsDefinitelySignaled()) {
      return i;
    }
  }

  internal::ScopedBlockingCallWithBaseSyncPrimitives scoped_blocking_call(
      FROM_HERE, BlockingType::WILL_BLOCK);

  const size_t signaled_id = WaitManyImpl(events);
  return signaled_id;
}
```
