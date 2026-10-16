---
name: general/document-sentinel-field-and-state-invariants
rubric: readability
severity: nit
---

**Rule.** When declaring member fields, optional handles, or nullable variables
(such as @Nullable Handler in Java or std::optional<T> in C++) that serve as
sentinels for feature activation or runtime state, add a comment documenting the
exact invariants and preconditions guaranteed to hold when the field is non-null
or set. Clearly document what setup, dependencies, or feature flags are verified
upon activation to distinguish runtime activation from static feature
enablement.

**Why.** Nullable sentinel fields and optional handles implicitly encode complex
lifecycle and initialization guarantees. Documenting what invariants hold when
the field is populated—such as required feature flag states and initialized
dependencies—clarifies the contract for readers, avoids conflating feature
availability with active execution, and prevents subtle misuse or null-pointer
bugs.

## Examples

Bad:

```cpp
// Bad: Unclear whether null indicates the feature is disabled vs not yet activated.
private static volatile @Nullable Handler sLauncherHandler;
```

Good:

```cpp
// Good: Clearly documents the invariants guaranteed when the sentinel is populated.
// If the ScopedBatchUpdate feature is activated, sLauncherHandler is non-null. When
// activated, it is guaranteed that isFeatureEnabled() is true and the binding request
// queue is non-null.
private static volatile @Nullable Handler sLauncherHandler;
```
