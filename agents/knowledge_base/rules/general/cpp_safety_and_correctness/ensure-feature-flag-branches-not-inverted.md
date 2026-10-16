---
name: general/ensure-feature-flag-branches-not-inverted
rubric: correctness
severity: should_fix
---

**Rule.** When branching on a feature flag or experiment predicate (such as
`base::FeatureList::IsEnabled(...)` or custom `Is*Enabled()` helpers) in
conditional statements or ternary expressions (`? :`), verify that branches are
not inverted. Ensure the enabled (true) branch selects the experimental or
optimized code path, and the disabled (false) branch retains the baseline
default behavior.

**Why.** Inverting feature flag branches causes experimental code to run when
the feature is disabled (the default for unlaunched features) and baseline code
to run when enabled. This invalidates A/B experimentation metrics and risks
exposing default users to unvetted behavior.

## Examples

Bad:

```cpp
// BAD: Inverted ternary branches execute experimental code when the feature is disabled.
VariantMap()
    : VariantMap(base::features::IsReducePPMsEnabled()
                     ? MapType::kStdMap
                     : MapType::kFlatHashMap) {}
```

Good:

```cpp
// GOOD: Enabled branch activates experimental code; disabled branch retains baseline std::map.
VariantMap()
    : VariantMap(base::features::IsReducePPMsEnabled()
                     ? MapType::kFlatHashMap
                     : MapType::kStdMap) {}
```
