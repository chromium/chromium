---
name: general/avoid-parameters-when-only-single-value-supported
rubric: design
severity: should_fix
---

**Rule.** Do not declare a parameter or configuration option (such as a state
enum, mode flag, or operation type) when only a single value or operation is
actually supported or implemented. Name the function specifically after the
supported operation (e.g., `Disable...()`) and configure the fixed state
internally instead of exposing a configurable parameter.

**Why.** Accepting a general state or mode parameter when only one value is
supported creates misleading API contracts, giving callers the false impression
that other options are valid. It requires unnecessary runtime validation or
error handling and risks silent failures or bugs if unsupported values are
passed.

## Examples

Bad:

```cpp
// Bad: Exposing an enum parameter when only disable is supported.
bool FeatureList::UpdateRuntimeMutableFeature(
    std::string_view feature_name,
    OverrideState overridden_state) {
  if (overridden_state != OVERRIDE_DISABLE_FEATURE) {
    // Only disabling features at runtime is supported.
    return false;
  }
  ApplyFeatureOverride(feature_name, overridden_state);
  return true;
}
```

Good:

```cpp
// Good: Name the function specifically for the supported operation and omit the parameter.
bool FeatureList::DisableRuntimeMutableFeature(
    std::string_view feature_name) {
  constexpr OverrideState target_state = OVERRIDE_DISABLE_FEATURE;
  ApplyFeatureOverride(feature_name, target_state);
  return true;
}
```
