---
name: general/document-thread-local-scope-and-lifecycle-for-static-setters
rubric: readability
severity: should_fix
---

**Rule.** When declaring static methods, configuration setters,
pre-initialization hooks, or test-reset and cleanup helpers that operate on
thread-local or sequence-local state, explicitly document their per-thread scope
and lifecycle semantics in header comments. Clarify whether modifications apply
strictly to the calling thread or main thread rather than process-wide, and
specify whether the configuration is persistent, one-shot (e.g., consumed and
reset on first access), or intended for scoped test teardown.

**Why.** Callers naturally expect static methods and class-level resetters to
affect the entire process globally. Explicitly documenting thread-local scope,
calling-thread restrictions, and one-shot consumption prevents callers from
assuming process-wide persistence or cross-thread test teardown, avoiding subtle
concurrency bugs and test pollution.

## Examples

Bad:

```cpp
// Bad: Header comments do not clarify thread-local scope, calling-thread restrictions, or lifecycle.
class MessagePumpUIApplication {
 public:
  // Allows host app to sync their nesting state before message pump starts.
  static void SetInitialNestingLevel(int depth);
};

class RunLoop {
 public:
  // Resets the thread-local state of the spin-loop detector.
  static void ResetSpinDetectorForTesting();
};
```

Good:

```cpp
// Good: Header comments explicitly document thread-local scope and lifecycle semantics.
class MessagePumpUIApplication {
 public:
  // Sets the initial run loop nesting `depth` for the current thread.
  // This is a thread_local and one-shot configuration; it applies only to the
  // next message pump initialized on this thread and is automatically reset.
  static void SetNextInitialNestingLevelForCurrentThread(int depth);
};

class RunLoop {
 public:
  // Resets the thread-local state of the spin-loop detector on the main thread.
  // Called by ~TaskEnvironment to prevent hollow RunUntilIdle() calls from leaking.
  static void ResetSpinDetectorForTesting();
};
```
