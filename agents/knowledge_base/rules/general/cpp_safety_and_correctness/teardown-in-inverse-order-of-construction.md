---
name: general/teardown-in-inverse-order-of-construction
rubric: safety
severity: nit
---

**Rule.** Perform destruction, teardown, resetting, and unregistration steps in
the exact inverse order of the corresponding construction, initialization, or
registration steps. When declaring class members or tearing down dependent
objects, ensure that longer-lived resources and dependencies are declared first
(destroyed last) so observed objects always outlive their observers.

**Why.** Subsequent initialization and registration steps often depend on state,
listeners, or context established by earlier steps. Tearing down resources in
the reverse order of creation ensures nested lifetimes, preserving valid
dependency references and preventing dangling pointer or use-after-free bugs
during destruction.

## Examples

Bad:

```cpp
void ThreadGroupProfiler::OnWorkerThreadExitingTask(
    internal::WorkerThread* worker_thread,
    WaitableEvent* profiling_has_stopped) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(task_runner_sequence_checker_);
  // Erasing context first leaves active collection with a dangling reference.
  worker_thread_context_set_.erase(worker_thread);
  if (active_collection_) {
    active_collection_->RemoveWorkerThread(worker_thread);
  }
  profiling_has_stopped->Signal();
}
```

Good:

```cpp
void ThreadGroupProfiler::OnWorkerThreadExitingTask(
    internal::WorkerThread* worker_thread,
    WaitableEvent* profiling_has_stopped) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(task_runner_sequence_checker_);
  // Teardown in reverse order of initialization/registration.
  if (active_collection_) {
    active_collection_->RemoveWorkerThread(worker_thread);
  }
  worker_thread_context_set_.erase(worker_thread);
  profiling_has_stopped->Signal();
}
```
