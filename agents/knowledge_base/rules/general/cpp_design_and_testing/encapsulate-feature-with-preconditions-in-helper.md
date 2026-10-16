---
name: general/encapsulate-feature-with-preconditions-in-helper
rubric: design
severity: should_fix
---

**Rule.** When a base::Feature requires hardware, device, or platform
preconditions before it can take effect, encapsulate the raw base::Feature in an
internal namespace or private header and expose an Is<Feature>Enabled() helper
function. Ensure all eligibility preconditions are validated before evaluating
base::FeatureList::IsEnabled().

**Why.** Calling base::FeatureList::IsEnabled() registers the client into a
Finch study group (control or experiment). If callers query the raw feature
before or without validating eligibility preconditions, ineligible clients
register in Finch studies and corrupt experimentation metrics.

## Examples

Bad:

```cpp
// Exposing raw feature publicly allows callers to query it directly,
// risking premature Finch enrollment or omitting precondition checks.
BASE_EXPORT BASE_DECLARE_FEATURE(kRestrictBigCoreThreadAffinity);

// Caller must remember to verify preconditions at every callsite:
if (base::IsEligibleForBigCoreAffinityChange() &&
    base::FeatureList::IsEnabled(base::kRestrictBigCoreThreadAffinity)) {
  ApplyBigCoreRestriction();
}
```

Good:

```cpp
// Encapsulate raw feature in an internal namespace and expose a helper function.
namespace internal {
BASE_EXPORT BASE_DECLARE_FEATURE(kRestrictBigCoreThreadAffinity);
}  // namespace internal

BASE_EXPORT bool IsRestrictBigCoreThreadAffinityEnabled() {
  return IsEligibleForBigCoreAffinityChange() &&
         base::FeatureList::IsEnabled(internal::kRestrictBigCoreThreadAffinity);
}
```
