---
name: testing/list-only-launch-candidate-experiment-in-testing-config
rubric: testing
severity: should_fix
---

**Rule.** In `testing/variations/fieldtrial_testing_config.json`, configure only
the single experiment group most likely to launch for each platform entry, or
ensure the primary launch candidate is ordered as the first entry in the
`experiments` array. Do not place secondary or alternative experiment groups
ahead of the intended launch configuration.

**Why.** Waterfall test bots, browser tests, and Web Platform Tests execute only
the first entry in the `experiments` array for any targeted platform. Placing
secondary experiment groups first or listing inactive groups leaves the intended
rollout configuration unexercised and clutters the test configuration.

## Examples

Bad:

```cpp
"experiments": [
  // Anti-pattern: secondary variant listed before primary launch candidate.
  {
    "name": "SecondaryFallbackGroup",
    "enable_features": ["FeatureA"]
  },
  {
    "name": "PrimaryLaunchCandidate",
    "enable_features": ["FeatureA", "FeatureB"]
  }
]
```

Good:

```cpp
"experiments": [
  // Place the primary launch candidate first (or list it exclusively).
  {
    "name": "PrimaryLaunchCandidate",
    "enable_features": ["FeatureA", "FeatureB"]
  },
  {
    "name": "SecondaryFallbackGroup",
    "enable_features": ["FeatureA"]
  }
]
```
