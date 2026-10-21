---
name: base/preserve-delay-in-test-task-executors
rubric: design
severity: should_fix
---

**Rule.** When adding or modifying test hooks to intercept delayed tasks (such
as pre-native background task runners), do not use java.util.concurrent.Executor
which drops delay parameters. Use a dedicated interface that accepts both the
task and delay (e.g., void scheduleDelayedTask(Runnable task, long delay)) so
test frameworks can simulate timing via fake-clock mechanisms like
Handler.postDelayed().

**Why.** java.util.concurrent.Executor.execute() only accepts a Runnable,
dropping delay information and forcing delayed tasks to execute immediately or
without timing metadata. Passing the delay through a specialized interface
allows test runners (such as Robolectric) to route delayed tasks through
simulated clock advancement mechanisms like ShadowLooper.

## Examples

Bad:

```cpp
// Executor drops the delay parameter, executing delayed tasks immediately in tests.
Executor executor = PostTask.getPrenativeDelayedThreadPoolExecutor();
if (executor != null) {
    executor.execute(task);
    return true;
}
```

Good:

```cpp
// Dedicated interface preserves task delay for simulated clock scheduling in tests.
PostTask.DelayedExecutorForTesting delayedExecutor =
        PostTask.getPrenativeDelayedThreadPoolExecutor();
if (delayedExecutor != null) {
    delayedExecutor.scheduleDelayedTask(task, delay);
    return true;
}
```
