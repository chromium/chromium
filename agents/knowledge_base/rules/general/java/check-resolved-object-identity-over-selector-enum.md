---
name: general/check-resolved-object-identity-over-selector-enum
rubric: readability
severity: nit
---

**Rule.** When an object instance is selected based on an enum, state parameter,
or configuration key and operated on, guard subsequent operations on a default
or fallback instance by comparing the resolved object reference directly
(`target != default_target`) rather than re-evaluating the input enum condition
(`state != State::kDefault`).

**Why.** Comparing resolved object references directly reflects the actual
runtime state (whether the fallback instance was already operated on), decouples
the guard from enum-to-object mapping logic, and makes the reason for the check
immediately evident at the call site.

## Examples

Bad:

```cpp
ChildServiceConnection binding = resolveBinding(initialBindingState);
binding.bindServiceConnection();

// Bad: Re-evaluating the input enum instead of checking the resolved instance.
if (initialBindingState != ChildBindingState.WAIVED) {
    mWaivedBinding.bindServiceConnection();
}
```

Good:

```cpp
ChildServiceConnection binding = resolveBinding(initialBindingState);
binding.bindServiceConnection();

// Good: Comparing the resolved object reference directly against the fallback instance.
if (binding != mWaivedBinding) {
    mWaivedBinding.bindServiceConnection();
}
```
