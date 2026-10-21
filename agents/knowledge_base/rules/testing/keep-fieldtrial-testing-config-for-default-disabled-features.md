---
name: testing/keep-fieldtrial-testing-config-for-default-disabled-features
rubric: correctness
severity: should_fix
---

**Rule.** When enabling a `base::Feature` by default
(`base::FEATURE_ENABLED_BY_DEFAULT`) or removing an obsolete feature flag,
remove its corresponding experiment or study entry from
`testing/variations/fieldtrial_testing_config.json`. In
`fieldtrial_testing_config.json`, do not remove study configurations or
individual platforms from `platforms` during cleanup or feature changes if any
enabled feature remains disabled by default (`FEATURE_DISABLED_BY_DEFAULT`),
defined in external repositories (such as ChromeOS), or actively experimented on
via Finch on that platform. Conversely, omit feature flags that have launched
everywhere (enabled by default) from `enable_features`, and cleanly delete any
experiments or entire study sections that become empty or obsolete, verifying
that test suites continue to pass without them.

**Why.** `fieldtrial_testing_config.json` ensures waterfall bots and test suites
exercise unlaunched features under active experimentation across targeted
platforms. Removing study configurations or platforms while an experiment is
active or default-disabled silently drops test coverage for enrolled users,
while leaving launched features in configs or retaining empty/obsolete studies
creates obsolete test overrides and clutters test configurations with dead code.

## Examples

Bad:

```cpp
// Bad 1: Enabling feature by default in C++ while leaving obsolete study in fieldtrial_testing_config.json:
BASE_FEATURE(kAutofillSupportPhoneticNameForJP, base::FEATURE_ENABLED_BY_DEFAULT);
// testing/variations/fieldtrial_testing_config.json still contains "AutofillSupportPhoneticNameForJP"

// Bad 2: Deleting a study while a feature is still default-disabled:
-    "Collections": [
-        {"platforms": ["android"], "experiments": [{"name": "Dogfood", "enable_features": ["ShoppingList"]}]}
-    ]

// Bad 3: Removing an active experiment platform while Finch trial is still running:
    "PartitionAllocFreeWithSize": [
        {
            "platforms": ["android", "android_webview", /* removed chromeos */ "ios", "linux", "mac", "windows"]

// Bad 4: Listing launched features under enable_features or retaining obsolete studies:
    "AutofillAi": [
        {"platforms": ["android"], "experiments": [{"name": "Enabled", "enable_features": ["AutofillAiLaunchedByDefault"]}]}
    ]
```

Good:

```cpp
// Good 1: Removing fieldtrial_testing_config.json entry when enabling feature by default in C++:
BASE_FEATURE(kAutofillSupportPhoneticNameForJP, base::FEATURE_ENABLED_BY_DEFAULT);
// In testing/variations/fieldtrial_testing_config.json:
-    "AutofillSupportPhoneticNameForJP": [ ... ]

// Good 2: Retaining study config while any feature is FEATURE_DISABLED_BY_DEFAULT:
    "Collections": [
        {"platforms": ["android"], "experiments": [{"name": "Dogfood", "enable_features": ["ShoppingList"]}]}
    ]

// Good 3: Retaining active platforms in platforms array to maintain test coverage:
    "PartitionAllocFreeWithSize": [
        {"platforms": ["android", "android_webview", "chromeos", "ios", "linux", "mac", "windows"]}

// Good 4: Omitting launched features and cleanly deleting empty/obsolete study sections.
```
