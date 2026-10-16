---
name: general/document-escape-hatch-and-edge-case-methods
rubric: readability
severity: should_fix
---

**Rule.** When providing or retaining specialized escape-hatch, lower-level, or
safety-bypass methods, range adapters, or invariant relaxation helpers alongside
primary APIs, explicitly document in the declaration doc comment what check or
invariant is bypassed and the specific scenarios, valid usage conditions, and
caller preconditions required for safe invocation. Avoid generic or tautological
comments that merely repeat the method name or leave escape hatches without
clear guidance on when they are safe to use.

**Why.** Without explicit doc comments clarifying that a method is a specialized
escape hatch or safety bypass and detailing its intended scenarios and caller
preconditions, callers may inadvertently invoke the lower-level method instead
of standard high-level APIs or misuse the bypass, violating subsystem
invariants, triggering unexpected reentrancy, or introducing subtle safety bugs.

## Examples

Bad:

```cpp
// 1. Escape-hatch API lacking usage scenario criteria:
// Directly sets the SingleThreadTaskRunner that will be returned by
// SingleThreadTaskRunner::GetCurrentDefault on the main thread.
virtual void SetDefaultTaskRunner(
    scoped_refptr<SingleThreadTaskRunner> task_runner,
    TaskQueue::QueuePriority priority) = 0;

// 2. Safety bypass with tautological doc comment:
// Method to return the range adapter
ReentrantRange GetReentrantRange() {
  return ReentrantRange{Iter</*check_reentrancy=*/false>(this),
                        Iter</*check_reentrancy=*/false>()};
}
```

Good:

```cpp
// 1. Explicitly documents edge-case status and valid scenarios:
// Directly sets the SingleThreadTaskRunner that will be returned by
// SingleThreadTaskRunner::GetCurrentDefault on the main thread. Use this only
// when setting a task runner that is not a TaskQueue's default, e.g. one with
// a custom task type.
virtual void SetDefaultTaskRunner(
    scoped_refptr<SingleThreadTaskRunner> task_runner,
    TaskQueue::QueuePriority priority) = 0;

// 2. Explains bypassed invariant, safe conditions, and caller preconditions:
// Method to return the reentrant range adapter. Use this if it is safe to
// iterate observers even if the observer list itself is non-reentrant.
// Callers must ensure that observer callbacks will not mutate the list.
ReentrantRange GetReentrantRange() {
  return ReentrantRange{Iter</*check_reentrancy=*/false>(this),
                        Iter</*check_reentrancy=*/false>()};
}
```
