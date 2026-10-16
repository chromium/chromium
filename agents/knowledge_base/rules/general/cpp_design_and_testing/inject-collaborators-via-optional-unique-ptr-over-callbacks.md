---
name: general/inject-collaborators-via-optional-unique-ptr-over-callbacks
rubric: design
severity: should_fix
---

**Rule.** Prefer dependency injection of collaborator objects via an optional
std::unique_ptr<Collaborator> = nullptr constructor parameter with fallback to
default construction when null, rather than introducing custom callback type
aliases and static default-factory methods. When callers omit the collaborator
or pass nullptr, instantiate the default implementation in the constructor body.

**Why.** Wrapping collaborator creation in custom callback type aliases and
static factory methods introduces unnecessary abstraction, indirection, and
header boilerplate. Using optional std::unique_ptr constructor parameters keeps
production call sites clean while allowing tests to inject mocks or
test-specific subclasses directly.

## Examples

Bad:

```cpp
// thread_group_profiler.h
using GetTimeToNextCollectionCallback = RepeatingCallback<TimeDelta()>;

explicit ThreadGroupProfiler(
    scoped_refptr<SequencedTaskRunner> task_runner,
    GetTimeToNextCollectionCallback time_to_next_collection_callback =
        GetDefaultTimeToNextCollectionCallback());

// thread_group_profiler.cc
ThreadGroupProfiler::GetTimeToNextCollectionCallback
ThreadGroupProfiler::GetDefaultTimeToNextCollectionCallback() {
  return BindRepeating(
      &PeriodicSamplingScheduler::GetTimeToNextCollection,
      Owned(std::make_unique<PeriodicSamplingScheduler>()));
}
```

Good:

```cpp
// thread_group_profiler.h
explicit ThreadGroupProfiler(
    scoped_refptr<SequencedTaskRunner> task_runner,
    std::unique_ptr<PeriodicSamplingScheduler> periodic_sampling_scheduler =
        nullptr);

// thread_group_profiler.cc
ThreadGroupProfiler::ThreadGroupProfiler(
    scoped_refptr<SequencedTaskRunner> task_runner,
    std::unique_ptr<PeriodicSamplingScheduler> periodic_sampling_scheduler)
    : periodic_sampling_scheduler_(std::move(periodic_sampling_scheduler)) {
  if (!periodic_sampling_scheduler_) {
    periodic_sampling_scheduler_ =
        std::make_unique<PeriodicSamplingScheduler>();
  }
}
```
