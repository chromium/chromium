---
name: general/do-not-rely-on-java-assert-for-production-validation
rubric: correctness
severity: should_fix
---

**Rule.** Do not rely on Java assert statements for input validation, API
preconditions, or error conditions that must be enforced in production. Use
explicit if checks (returning an error status, falling back gracefully, or
throwing an exception) instead.

**Why.** In Chromium Android builds, R8 strips Java assert statements in release
builds (enabling them only in debug and Canary builds). Relying on assert for
parameter validation or error handling allows production builds to continue with
invalid state, causing silent failures or unpredictable behavior.

## Examples

Bad:

```cpp
assert initialBindingState != ChildBindingState.UNBOUND
        : "UNBOUND is not a valid initial binding state.";
```

Good:

```cpp
if (initialBindingState == ChildBindingState.UNBOUND) {
    return false;
}
```
