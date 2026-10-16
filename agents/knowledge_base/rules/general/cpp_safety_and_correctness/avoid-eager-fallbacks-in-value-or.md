---
name: general/avoid-eager-fallbacks-in-value-or
rubric: safety
severity: should_fix
---

**Rule.** Do not pass expressions with side effects, non-trivial function calls,
or allocating operations to std::optional::value_or(). Evaluate computed
fallback expressions lazily using a conditional check (such as
`opt ? *opt : Fallback()`) or an if statement.

**Why.** In C++, arguments passed to std::optional::value_or() are evaluated
eagerly and unconditionally before the function is called. Passing non-trivial
expressions or function calls wastes computation, risks unexpected side effects,
and can trigger redundant I/O or allocations even when the optional already
contains a valid value.

## Examples

Bad:

```cpp
// BAD: GetExplorerPid() is called eagerly even when explorer_process_id has a value.
const ProcessId pid = explorer_process_id.value_or(GetExplorerPid());
```

Good:

```cpp
// GOOD: Fallback function is lazily evaluated only when the optional is disengaged.
const ProcessId pid =
    explorer_process_id ? *explorer_process_id : GetExplorerPid();
```
