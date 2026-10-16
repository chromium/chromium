---
name: general/keep-utility-classes-platform-agnostic
rubric: design
severity: should_fix
---

**Rule.** Keep utility and helper classes platform-agnostic rather than gating
internal mechanisms or member operations behind #if BUILDFLAG(...) compile-time
guards, especially when behavior is already parameterized via constructor
arguments or injected delegates. Centralize platform-specific activation
policies in the caller or startup configuration layer as the single source of
truth.

**Why.** Gating lower-level utility mechanisms with compile-time platform build
guards causes #ifdef fragmentation, prevents code reuse and testing across
platforms, and tightly couples low-level utilities with platform policy
decisions. Keeping utilities parameter-driven allows callers to control platform
activation and makes the class fully testable on all platforms.

## Examples

Bad:

```cpp
// Bad: internal mechanism conditionally compiled out despite caller parameter
PerfettoTaskRunner::PerfettoTaskRunner(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    bool defer_delayed_tasks)
    : task_runner_(std::move(task_runner)) {
#if BUILDFLAG(IS_POSIX)
  defer_delayed_tasks_ = defer_delayed_tasks;
#endif
}

void PerfettoTaskRunner::PostDelayedTask(std::function<void()> task,
                                         uint32_t delay_ms) {
#if BUILDFLAG(IS_POSIX)
  if (delay_ms && defer_delayed_tasks_) {
    deferred_delayed_tasks_.emplace_back(task, delay_ms);
    return;
  }
#endif  // BUILDFLAG(IS_POSIX)
  task_runner_->PostDelayedTask(std::move(task), delay_ms);
}
```

Good:

```cpp
// Good: utility is platform-agnostic; caller sets defer_delayed_tasks per platform
PerfettoTaskRunner::PerfettoTaskRunner(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    bool defer_delayed_tasks)
    : task_runner_(std::move(task_runner)),
      defer_delayed_tasks_(defer_delayed_tasks) {
  CHECK(task_runner_);
}

void PerfettoTaskRunner::PostDelayedTask(std::function<void()> task,
                                         uint32_t delay_ms) {
  if (defer_delayed_tasks_ && delay_ms) {
    deferred_delayed_tasks_.emplace_back(task, delay_ms);
    return;
  }
  task_runner_->PostDelayedTask(std::move(task), delay_ms);
}
```
