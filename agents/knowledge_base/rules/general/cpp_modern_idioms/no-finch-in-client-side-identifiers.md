---
name: general/no-finch-in-client-side-identifiers
rubric: readability
severity: should_fix
---

**Rule.** Do not use Google-internal codenames such as 'Finch' in open-source
Chromium code, identifiers (classes, methods, variables), comments,
documentation, or commit messages. Use public Chromium terminology
('base::FeatureList', 'FieldTrial', 'variations', 'base::Feature', 'field trial
testing config') or describe the concrete mechanism ('feature param',
'client-side flag') instead.

**Why.** Chromium is an open-source project with multiple embedders. 'Finch'
refers specifically to Google's internal experiment server infrastructure rather
than public Chromium client APIs or client-side testing configurations
(`fieldtrial_testing_config.json`). Using standard terminology like
'FeatureList', 'variations', 'field trials', or 'field trial testing config'
ensures consistency, avoids leaking internal codenames, and clarifies whether
client, test, or server mechanisms are being referenced.

## Examples

Bad:

```cpp
// Bad: Using internal codename 'Finch' in identifiers, comments, or commit messages
public final class InitializeFinch {
    public static void initializeFinch() {
        InitializeFinchJni.get().initializeFinch();
    }
}

// Processes with crash reporting disabled are controlled by Finch.
inline constexpr base::FeatureParam<bool> kInitFinchEarly(&kFeature, "init_finch", true);

// In commit message:
// Add Finch config for PreviewHandwritingGesture on Android
```

Good:

```cpp
// Good: Using standard public terminology ('FeatureList', 'feature param', 'field trial testing config')
public final class InitializeFeatureList {
    public static void initializeFeatureList() {
        InitializeFeatureListJni.get().initializeFeatureList();
    }
}

// Processes with crash reporting disabled are controlled via the feature param.
inline constexpr base::FeatureParam<bool> kInitFeatureListEarly(&kFeature, "init_feature_list", true);

// In commit message:
// Add field trial testing config for PreviewHandwritingGesture
```
