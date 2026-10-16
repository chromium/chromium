---
name: general/document-ratcheting-rollout-plan-in-migration-commit-message
rubric: readability
severity: should_fix
---

**Rule.** In commit messages for multi-step migrations tightening safety checks,
template traits, or defaults (such as dangling pointer detection or raw_ptr
traits), describe the ratcheting rollout plan rather than an opt-in model.
Explain the planned phases: (1) annotating or allowlisting pre-existing
violators, (2) switching the default to the strict/safe setting to lock in
compliance and prevent new regressions, and (3) triaging and resolving marked
instances individually.

**Why.** Hardening APIs or safety checks by asking callers to opt in leaves the
codebase vulnerable to new regressions. Explaining the ratcheting strategy in
commit messages clarifies migration intent for reviewers and ensures
pre-existing violations are explicitly allowlisted so the safe default can be
locked in tree-wide.

## Examples

Bad:

```cpp
In order to reduce the number of base::ScopedObservation<...> identified
as dangling, pass the RawPtrTraits as parameter of the template. It will
then be possible to mark non dangling ones by passing kEmpty instead.
```

Good:

```cpp
In order to reduce the number of base::ScopedObservation<...> identified
as dangling, pass the RawPtrTraits as parameter of the template.

The followup is to identify the dangling instances, mark them as being
LeakedDanglingUntriaged, and set the default to kEmpty. Then dangling
instances can be fixed individually.
```
