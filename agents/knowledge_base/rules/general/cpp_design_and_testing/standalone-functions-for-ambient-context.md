---
name: general/standalone-functions-for-ambient-context
rubric: design
severity: should_fix
---

**Rule.** When an operation queries ambient, thread-local, or global execution
context (such as the currently running task, active sequence, thread importance,
or ambient isolate) rather than class-specific state, define it as a standalone
non-member function in an appropriate namespace rather than as a static member
function on a class.

**Why.** In C++, static member functions can be called through instance member
access syntax (e.g. `instance->StaticMethod()`), which misleadingly suggests to
readers that the query inspects or depends on that specific object instance.
Defining ambient context queries as standalone non-member functions prevents
misleading member-access syntax and clearly documents that the operation
inspects ambient execution context.

## Examples

Bad:

```cpp
class BASE_EXPORT TaskRunner : public RefCountedThreadSafe<TaskRunner> {
 public:
  // Misleading: can be invoked as runner->GetCurrentThreadType(),
  // falsely implying it inspects this TaskRunner instance.
  static ThreadType GetCurrentThreadType();
};
```

Good:

```cpp
namespace base::internal {
// Standalone non-member function clarifies that the query inspects
// ambient execution context rather than a specific TaskRunner instance.
ThreadType GetCurrentTaskImportance();
}  // namespace base::internal
```
