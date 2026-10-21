---
name: testing/use-disable-features-for-default-enabled-features
rubric: readability
severity: should_fix
---

**Rule.** When configuring experiments in
`testing/variations/fieldtrial_testing_config.json` to test with a feature
disabled—either as an ablation/holdback/kill switch for a default-enabled
feature or as a specific experiment arm variant—explicitly list the feature
under `disable_features`. For default-enabled features, use an experiment group
name like `Disabled` or `Holdback` instead of `enable_features` and `Enabled`.
For experiment arms requiring a feature to be off, do not rely on its C++
default-disabled state implicitly.

**Why.** `fieldtrial_testing_config.json` configures waterfall test bots, Web
Platform Tests, and trybots. Listing a default-enabled feature under
`enable_features` is a no-op that fails to exercise the intended disabled state.
Furthermore, explicitly declaring `disable_features` ensures experiment arms do
not depend on client-side default states that could change and keeps test
configurations consistent with server-side Finch experiment definitions.

## Examples

Bad:

```cpp
// In testing/variations/fieldtrial_testing_config.json:
// Bad: Using enable_features for a default-enabled feature ablation.
{
    "name": "Enabled",
    "enable_features": [
        "V8Flag_compaction_on_regular_gcs"
    ]
}
// Bad: Relying on C++ default-disabled state instead of explicit disable_features.
{
    "name": "EnabledGraphiteContextPerThread",
    "enable_features": [
        "EnableDrDc"
    ]
}
```

Good:

```cpp
// In testing/variations/fieldtrial_testing_config.json:
// Good: Using disable_features with a Disabled group name for default-enabled feature.
{
    "name": "Disabled",
    "disable_features": [
        "V8Flag_compaction_on_regular_gcs"
    ]
}
// Good: Explicitly listing disable_features for the experiment arm variant.
{
    "name": "EnabledGraphiteContextPerThread",
    "enable_features": [
        "EnableDrDc"
    ],
    "disable_features": [
        "GraphiteContextIsThreadSafe"
    ]
}
```
