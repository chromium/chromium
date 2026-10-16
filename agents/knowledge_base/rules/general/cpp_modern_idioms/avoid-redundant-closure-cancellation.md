---
name: general/avoid-redundant-closure-cancellation
rubric: readability
severity: nit
---

**Rule.** When managing cancelable closures (such as
base::CancelableOnceClosure) or timers with documented active/inactive state
invariants, explicitly cancel or reset them on all constructor and
initialization branches where they are not scheduled. In downstream completion
callbacks and terminal handlers, rely on these established invariants and omit
redundant Cancel() or Stop() calls.

**Why.** Explicitly cancelling or disarming unused closures during
initialization ensures member state matches documented contracts and prevents
bugs if callers check IsCancelled(). Relying on established invariants
downstream avoids misleading redundant dead code in completion handlers.

## Examples

Bad:

```cpp
// Initialization: leaves closure uncancelled when not scheduled.
if (profilers_.empty()) {
  task_runner_->PostDelayedTask(
      FROM_HERE, empty_collection_closure_.callback(), sampling_duration_);
}

// Completion callback: redundant cancellation.
void OnProfilerCollectionCompleted(WorkerThread* thread) {
  empty_collection_closure_.Cancel();
  profilers_.erase(thread);
}
```

Good:

```cpp
// Initialization: explicitly cancels closure on alternate branch to maintain invariant.
if (profilers_.empty()) {
  task_runner_->PostDelayedTask(
      FROM_HERE, empty_collection_closure_.callback(), sampling_duration_);
} else {
  empty_collection_closure_.Cancel();
}

// Completion callback: trusts established invariant and omits redundant Cancel().
void OnProfilerCollectionCompleted(WorkerThread* thread) {
  profilers_.erase(thread);
}
```
