---
name: general/document-threading-model-for-volatile-java-fields
rubric: readability
severity: optional
---

**Rule.** When declaring or modifying `volatile` variables or `Atomic*` fields
(such as `AtomicBoolean`, `AtomicInteger`, or `AtomicReference`) in Java for
lock-free thread synchronization, add a comment documenting the threading model.
Explicitly explain which threads or task runners (such as the UI/main thread,
background worker threads, or native launcher sequences) are expected to read
and write the field.

**Why.** Lock-free synchronization primitives in Java lack explicit compile-time
monitor annotations like `@GuardedBy`. Documenting which threads read and write
each variable clarifies concurrency contracts, prevents mistaken single-threaded
assumptions, and protects against data races during future refactorings.

## Examples

Bad:

```cpp
// Temporary boolean for an experiment crbug.com/489983480
// shuts down the thread pool when it is no longer needed.
private static volatile boolean sShutdownThreadPoolEnabled;

private final AtomicReference<State> mState = new AtomicReference<>(State.IDLE);
```

Good:

```cpp
// Temporary boolean for an experiment crbug.com/489983480
// shuts down the thread pool when it is no longer needed.
// Written by the UI thread during startup; read concurrently by background worker
// threads when dispatching tasks.
private static volatile boolean sShutdownThreadPoolEnabled;

// Updated on the background loader thread upon completion; read on the UI thread
// when querying active controller status.
private final AtomicReference<State> mState = new AtomicReference<>(State.IDLE);
```
