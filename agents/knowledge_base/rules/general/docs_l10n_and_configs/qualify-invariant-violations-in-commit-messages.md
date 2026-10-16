---
name: general/qualify-invariant-violations-in-commit-messages
rubric: readability
severity: should_fix
---

**Rule.** When describing invariants, safety assumptions, alignment guarantees,
or security properties (or their non-guarantees) in commit messages, explicitly
qualify their scope—such as the specific triggering condition, API, thread,
process, or rollout phase—rather than making broad, unqualified assertions. If a
guarantee generally holds or a limitation only applies to a specific launch
phase or execution context, document the precise scenario or boundary.

**Why.** Broad or unqualified assertions in commit messages mislead reviewers
and future maintainers into believing an invariant is universally broken or that
a security limitation applies across all processes and threads. Explicitly
qualifying the condition, API, thread, or rollout phase clarifies the true
architectural boundary and preserves accurate Git history.

## Examples

Bad:

```cpp
// Variant 1: Unqualified invariant violation
The former is flawed because it assumes that it can always obtain an aligned pointer for reading char16_t, but this is not true when the underlying payload pointer is not aligned to a multiple of 2.

// Variant 2: Unqualified security property disclaimer
During non-task related work we aren't claiming any security properties of MiracleObject.
```

Good:

```cpp
// Variant 1: Explicitly cite triggering condition/API
The former is flawed because it assumes that it can always obtain an aligned pointer for reading char16_t, but this is not true when the underlying payload pointer is not aligned to a multiple of 2, which can happen when WithUnownedBuffer is used.

// Variant 2: Explicitly qualify scope and rollout phase
During non-task related work we aren't claiming any security properties of MiracleObject for our current Browser UI thread launch.
```
