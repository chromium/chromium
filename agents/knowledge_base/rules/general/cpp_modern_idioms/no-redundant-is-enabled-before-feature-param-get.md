---
name: general/no-redundant-is-enabled-before-feature-param-get
rubric: readability
severity: nit
---

**Rule.** Do not redundantly check `base::FeatureList::IsEnabled(kFeature)`
before calling `base::FeatureParam<T>::Get()` or
`GetFieldTrialParamByFeature*()` when falling back to the parameter's configured
default value. Call `Get()` directly, as `base::FeatureParam` automatically
returns its default value when the associated feature is disabled or
unconfigured.

**Why.** `base::FeatureParam<T>::Get()` automatically evaluates to its default
value when the feature is disabled, unconfigured, or invalid. Checking
`FeatureList::IsEnabled()` before calling `Get()` adds redundant branching and
requires duplicating default constants at call sites.

## Examples

Bad:

```cpp
// Redundantly checks FeatureList::IsEnabled and duplicates the default value.
const base::TimeDelta timeout =
    base::FeatureList::IsEnabled(kMyFeature)
        ? kMyTimeoutParam.Get()
        : base::Seconds(10);
```

Good:

```cpp
// FeatureParam::Get() automatically returns its default value when disabled.
const base::TimeDelta timeout = kMyTimeoutParam.Get();
```
