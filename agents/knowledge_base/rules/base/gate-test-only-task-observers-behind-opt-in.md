---
name: base/gate-test-only-task-observers-behind-opt-in
rubric: safety
severity: should_fix
---

**Rule.** Do not attach base::TaskObserver instances unconditionally in
production code paths or run loops for test-only assertions, monitoring, or
spin-loop detection. Gate observer registration behind an explicit testing
opt-in flag enabled by test harnesses (such as base::test::TaskEnvironment or
content::BrowserTestBase).

**Why.** base::TaskObserver callbacks (WillProcessTask and DidProcessTask) run
on every task dispatched on a thread, introducing measurable incremental runtime
overhead on hot task execution paths. Gating test-only observers behind explicit
test harness initialization prevents unnecessary performance degradation in
production.

## Examples

Bad:

```cpp
void RunLoop::RunUntilIdle() {
  if (CurrentThread::IsSet()) {
    if (g_spin_detector.observer_depth == 0) {
      CurrentThread::Get()->AddTaskObserver(&g_spin_detector);
    }
    // ...
  }
}
```

Good:

```cpp
void RunLoop::RunUntilIdle() {
  if (g_spin_detector_enabled.load(std::memory_order_relaxed) &&
      CurrentThread::IsSet()) {
    if (g_spin_detector.observer_depth == 0) {
      CurrentThread::Get()->AddTaskObserver(&g_spin_detector);
    }
    // ...
  }
}
```
