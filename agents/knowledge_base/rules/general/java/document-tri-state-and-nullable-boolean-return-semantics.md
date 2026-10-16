---
name: general/document-tri-state-and-nullable-boolean-return-semantics
rubric: readability
severity: should_fix
---

**Rule.** When declaring or documenting methods or functions that return a
tri-state or nullable boolean (such as `@Nullable Boolean` in Java or
`std::optional<bool>` in C++), explicitly document the semantic meaning of
returning `null` or empty (`std::nullopt`). Clarify whether `null` indicates an
uninitialized state, lack of constraint, unknown or indeterminate value, or a
non-applicable condition alongside the `true` and `false` outcomes.

**Why.** A nullable or optional boolean represents three distinct logical states
rather than a simple binary choice. Without explicit documentation, callers
cannot determine what a null or empty return represents versus boolean false,
leading to incorrect assumptions, improper null checks, or unhandled states.

## Examples

Bad:

```cpp
// Java:
static <T> @Nullable Boolean allowsSetToNull(NullableObservableSupplier<T> thing) {
    if (thing instanceof BaseObservableSupplierImpl<T> impl) {
        return impl.mAllowSetToNull;
    }
    return !(thing instanceof ObservableSupplier<T>);
}

// C++:
// Returns whether the feature is supported.
std::optional<bool> IsFeatureSupported();
```

Good:

```cpp
// Java:
/**
 * Returns whether setting null is allowed. Returns null if |thing| does not specify a
 * constraint (caller should preserve current behavior), true if allowed, or false if forbidden.
 */
static <T> @Nullable Boolean allowsSetToNull(NullableObservableSupplier<T> thing) {
    if (thing instanceof BaseObservableSupplierImpl<T> impl) {
        return impl.mAllowSetToNull;
    }
    return !(thing instanceof ObservableSupplier<T>);
}

// C++:
// Returns true if supported, false if unsupported, or std::nullopt if the capability is
// indeterminate / unqueried.
std::optional<bool> IsFeatureSupported();
```
