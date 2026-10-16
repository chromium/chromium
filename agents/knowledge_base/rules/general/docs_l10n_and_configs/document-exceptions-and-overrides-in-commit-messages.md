---
name: general/document-exceptions-and-overrides-in-commit-messages
rubric: readability
severity: nit
---

**Rule.** When describing new, updated, or refactored behavior in commit message
bodies, explicitly specify any significant exceptions, override flags, opt-out
mechanisms, or boundary conditions that qualify the change. Do not state
behavioral changes unconditionally or omit override switches when certain
configurations, flags, or edge cases bypass the new logic.

**Why.** Unconditional or oversimplified summaries in commit descriptions
mislead code reviewers and cause confusion during regression triage, bisection,
or commit archaeology when investigating flag-specific behaviors or edge cases.

## Examples

Bad:

```cpp
This CL modifies the allocation path to use the system allocator even when kNoHooks is active.
```

Good:

```cpp
This CL modifies the allocation path to use the system allocator even when kNoHooks is active, unless kNoMemoryToolOverride is set.
```
