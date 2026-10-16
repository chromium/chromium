---
name: general/return-nullable-in-java-testing-getters
rubric: readability
severity: nit
---

**Rule.** In Java testing getters (such as `get*ForTesting()`) exposing a
nullable or lazily initialized field or collection, annotate the return type as
`@Nullable` and return the field directly. Do not synthesize throwaway fallback
instances (such as `new ObserverList<>()` or `Collections.emptyList()`) or
assert non-null (`assert != null`, `assumeNonNull(...)`) when null.

**Why.** Constructing empty collection instances on demand adds unnecessary
allocations and masks whether the underlying field was ever initialized.
Returning `@Nullable` keeps testing accessors simple, preserves actual
initialization state, and lets test code inspect or assert nullity directly.

## Examples

Bad:

```cpp
public static ObserverList<TaskVisibilityListener> getTaskVisibilityListenersForTesting() {
    return sTaskVisibilityListeners == null ? new ObserverList<>() : sTaskVisibilityListeners;
}
```

Good:

```cpp
public static @Nullable ObserverList<TaskVisibilityListener> getTaskVisibilityListenersForTesting() {
    return sTaskVisibilityListeners;
}
```
