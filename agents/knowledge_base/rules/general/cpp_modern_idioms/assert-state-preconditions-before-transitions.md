---
name: general/assert-state-preconditions-before-transitions
rubric: readability
severity: should_fix
---

**Rule.** Assert state preconditions and lifecycle invariants using CHECK() or
DCHECK() when transitioning, initializing, or tearing down state. Specifically,
verify that member pointers or optionals are unpopulated (e.g.,
CHECK(!active_session\_) or DCHECK(!active_collection\_)) before allocating or
emplacing state, and verify that resources are actively engaged or collections
are non-empty (e.g., DCHECK(active_collection\_) or
DCHECK(!profilers\_.empty())) before resetting state or removing items in
completion callbacks.

**Why.** Asserting preconditions verifies state-machine invariants during
execution and testing. Checking unpopulated state prevents re-entrancy and
silent overwrites, while asserting engaged optionals or non-empty collections
before resets and item removals prevents silent no-ops that mask duplicate
completion callbacks, out-of-order execution, or broken lifecycle transitions.

## Examples

Bad:

```cpp
void ThreadGroupProfiler::ActiveCollection::OnProfilerCollectionCompleted(
    internal::WorkerThread* worker_thread) {
  // Missing assertion that collection contains items before removal.
  profilers_.erase(worker_thread);
  if (profilers_.empty()) {
    std::move(collection_complete_callback_).Run();
  }
}

void ThreadGroupProfiler::CollectProfilesTask() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Missing assertion that previous session completed before emplace.
  active_collection_.emplace(context_, duration_, task_runner_);
}

void ThreadGroupProfiler::EndActiveCollectionTask() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Missing assertion that collection was active before reset.
  active_collection_.reset();
}
```

Good:

```cpp
void ThreadGroupProfiler::ActiveCollection::OnProfilerCollectionCompleted(
    internal::WorkerThread* worker_thread) {
  DCHECK(!profilers_.empty());
  profilers_.erase(worker_thread);
  if (profilers_.empty()) {
    std::move(collection_complete_callback_).Run();
  }
}

void ThreadGroupProfiler::CollectProfilesTask() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!active_collection_);
  active_collection_.emplace(context_, duration_, task_runner_);
}

void ThreadGroupProfiler::EndActiveCollectionTask() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(active_collection_);
  active_collection_.reset();
}
```
