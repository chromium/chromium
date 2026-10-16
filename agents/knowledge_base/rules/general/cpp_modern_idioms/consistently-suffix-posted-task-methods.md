---
name: general/consistently-suffix-posted-task-methods
rubric: readability
severity: nit
---

**Rule.** When a class adopts a naming convention (such as a \*Task suffix) for
member functions executed as asynchronous tasks on a task runner or sequence,
apply the suffix consistently across all sibling task methods. Do not mix
unsuffixed method names with suffixed task methods in the same class.

**Why.** Explicit and uniform task method naming makes asynchronous execution
context clear across class interfaces, distinguishes posted task handlers from
synchronous helper methods, and prevents confusion about which sequence or
thread a method runs on.

## Examples

Bad:

```cpp
class ThreadGroupProfiler {
 private:
  void StartTask();
  void OnWorkerThreadStartedTask(WorkerThread* worker_thread);
  void CollectProfiles();
  void EndActiveCollection();
};
```

Good:

```cpp
class ThreadGroupProfiler {
 private:
  void StartTask();
  void OnWorkerThreadStartedTask(WorkerThread* worker_thread);
  void CollectProfilesTask();
  void EndActiveCollectionTask();
};
```
