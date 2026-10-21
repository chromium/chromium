---
name: testing/enable-prerequisite-gating-params-in-testing-config
rubric: correctness
severity: should_fix
---

**Rule.** In `testing/variations/fieldtrial_testing_config.json`, when
configuring experiment groups and feature parameters for dark-launch, shadow,
dry-run, or secondary modes under `params`, ensure that parent gating or
prerequisite feature parameters are also set to `true` (or their activating
values). Do not disable parent gating parameters when enabling dependent
sub-modes if the underlying implementation gates entry to the feature or metric
collection on the parent parameter.

**Why.** Feature implementations often guard entire codepaths with an initial
check on a primary feature parameter before evaluating dark-launch or secondary
mode flags. Disabling the primary parameter causes methods to return early,
completely bypassing dark-launch execution, downstream validation, and telemetry
collection on test bots.

## Examples

Bad:

```cpp
{
  "name": "Enabled_DarkLaunch",
  "params": {
    "confirm_navigation_to_new_origins": "false",
    "confirm_navigation_to_new_origins_dark_launch": "true"
  },
  "enable_features": [
    "GlicCrossOriginNavigationGating"
  ]
}
```

Good:

```cpp
{
  "name": "Enabled_DarkLaunch",
  "params": {
    "confirm_navigation_to_new_origins": "true",
    "confirm_navigation_to_new_origins_dark_launch": "true"
  },
  "enable_features": [
    "GlicCrossOriginNavigationGating"
  ]
}
```
