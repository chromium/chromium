---
name: general/name-boolean-decision-functions-as-queries
rubric: readability
severity: should_fix
---

**Rule.** Name boolean query, decision, policy, or predicate methods using
indicative or interrogative phrasing (such as `Should...`, `Is...`, `Can...`,
`Has...`, `Allows...`, or `Supports...`) rather than imperative action verbs
(such as `Enroll...`, `Register...`, `Execute...`, or `Allow...`). Reserve
imperative action verbs for methods that execute mutating operations or
lifecycle actions, where a boolean return value indicates operation success or
failure.

**Why.** Imperative method names imply an action or mutating setter operation,
leading callers to mistake a read-only query for a state modification or
interpret a boolean return value as an operation status indicator. Indicative
predicate phrasing makes read-only boolean queries intuitive in conditional
expressions and prevents ambiguity at call sites.

## Examples

Bad:

```cpp
// C++: Imperative verb misleadingly implies an action whose return indicates success or failure.
bool EnrollInFeatureTrial() {
  return base::FeatureList::IsEnabled(kFeatureTrial);
}

// Java: Imperative verb sounds like a mutating setter rather than a read-only query.
static <T> Boolean allowSetToNull(NullableObservableSupplier<T> thing) {
  if (thing instanceof BaseObservableSupplierImpl<T> impl) {
    return impl.mAllowSetToNull;
  }
  return !(thing instanceof ObservableSupplier<T>);
}
```

Good:

```cpp
// C++: Query predicate clearly communicates policy decision in conditionals.
bool ShouldEnrollInFeatureTrial() {
  return base::FeatureList::IsEnabled(kFeatureTrial);
}

// Java: Indicative predicate clearly conveys a read-only property query.
static <T> @Nullable Boolean allowsSetToNull(NullableObservableSupplier<T> thing) {
  if (thing instanceof BaseObservableSupplierImpl<T> impl) {
    return impl.mAllowSetToNull;
  }
  return !(thing instanceof ObservableSupplier<T>);
}
```
