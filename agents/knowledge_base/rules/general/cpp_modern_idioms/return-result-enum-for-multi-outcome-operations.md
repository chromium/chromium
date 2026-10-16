---
name: general/return-result-enum-for-multi-outcome-operations
rubric: readability
severity: should_fix
---

**Rule.** When designing functions or methods that mutate state or perform
operations with multiple distinct outcomes (such as success, no-op or already
applied, and invalid argument or precondition failure), return a descriptive
result enum rather than `void` or a bare `bool`.

**Why.** Returning `void` silently discards outcome details, preventing callers
from knowing whether an operation succeeded, failed, or was a no-op. Returning a
bare `bool` conflates multiple distinct failure or non-success states (e.g.,
precondition failure vs. already in target state), hindering telemetry,
debugging, and caller error handling.

## Examples

Bad:

```cpp
// Bad: returning void or bool silently discards or conflates distinct outcomes.
void FeatureList::UpdateRuntimeMutableFeature(
    std::string_view feature_name,
    OverrideState target_state) {
  const auto* entry = GetOverrideEntry(feature_name);
  if (!entry || !entry->runtime_override_lock) {
    return;  // Precondition failed.
  }
  if (entry->overridden_state == target_state) {
    return;  // Already in desired state (no-op).
  }
  entry->overridden_state = target_state;
}
```

Good:

```cpp
// Good: return a descriptive enum class to distinguish outcome states.
enum class MutableFeatureUpdateResult {
  kSuccess,
  kAlreadySet,
  kInvalidFeature,
};

MutableFeatureUpdateResult FeatureList::UpdateRuntimeMutableFeature(
    std::string_view feature_name,
    OverrideState target_state) {
  const auto* entry = GetOverrideEntry(feature_name);
  if (!entry || !entry->runtime_override_lock) {
    return MutableFeatureUpdateResult::kInvalidFeature;
  }
  if (entry->overridden_state == target_state) {
    return MutableFeatureUpdateResult::kAlreadySet;
  }
  entry->overridden_state = target_state;
  return MutableFeatureUpdateResult::kSuccess;
}
```
