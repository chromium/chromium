---
name: general/name-thread-local-methods-with-for-current-thread
rubric: readability
severity: should_fix
---

**Rule.** When defining a static member function or free function that inspects,
retrieves, or mutates thread-local or per-thread state (such as thread_local
variables or base::ThreadLocalPointer), include ForCurrentThread in the
identifier (e.g., GetForCurrentThread(), Set...ForCurrentThread(),
Is...ForCurrentThread()). Do not use unqualified names that imply process-wide
or global state.

**Why.** Static methods and free functions without thread qualification appear
to operate on global, class-level, or process-wide state. Appending
ForCurrentThread makes per-thread scoping immediately obvious to callers at the
call site, preventing misleading assumptions about shared state and concurrency
hazards across threads.

## Examples

Bad:

```cpp
// Static methods inspecting or mutating thread-local state without thread scoping:
class MessagePumpUIApplication {
 public:
  static void SetInitialNestingLevel(int depth);
  static int GetInitialNestingLevel();
};
```

Good:

```cpp
// Explicitly scoped to the calling thread:
class MessagePumpUIApplication {
 public:
  static void SetNextInitialNestingLevelForCurrentThread(int depth);
  static int GetInitialNestingLevelForCurrentThread();
};
```
