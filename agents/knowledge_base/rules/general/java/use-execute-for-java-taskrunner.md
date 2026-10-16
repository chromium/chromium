---
name: general/use-execute-for-java-taskrunner
rubric: readability
severity: should_fix
---

**Rule.** When scheduling immediate tasks or designing task execution APIs in
Chromium Java, invoke execute(Runnable) on TaskRunner or Executor instances. Do
not add or call redundant method aliases like postTask(Runnable) on TaskRunner,
SequencedTaskRunner, or LocationAwareExecutor interfaces.

**Why.** In Chromium Android Java, TaskRunner extends LocationAwareExecutor
which implements java.util.concurrent.Executor, making execute(Runnable) the
canonical method for immediate task execution. Task execution methods on
LocationAwareExecutor undergo bytecode rewriting (LocationRewriter) to capture
caller source locations for tracing; adding alias methods expands API surface
and requires extra bytecode rewriter maintenance without providing new
functionality.

## Examples

Bad:

```cpp
// Adding alias method on interface:
public interface TaskRunner extends LocationAwareExecutor {
    default void postTask(Runnable task) {
        postDelayedTask(task, 0, null);
    }
}

// At caller:
taskRunner.postTask(() -> file.delete());
```

Good:

```cpp
// Use existing execute(Runnable) inherited from LocationAwareExecutor / Executor:
taskRunner.execute(() -> file.delete());
```
