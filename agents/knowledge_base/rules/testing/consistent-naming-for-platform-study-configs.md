---
name: testing/consistent-naming-for-platform-study-configs
rubric: readability
severity: should_fix
---

**Rule.** In testing/variations/fieldtrial_testing_config.json, name top-level
study entries after the overall feature flag without phase descriptors or
redundant Study suffixes. For platform-specific rollouts or platform-split
studies, include qualifying platform suffixes (e.g., ...Android, ...Ios,
...Desktop, or <Feature>StudyDesktop) to avoid ambiguity across concurrent
platform trials and match server-side FieldTrial configurations. In the
experiments array, use descriptive, symmetric group names: name phased rollout
groups after their specific stage (e.g., Phase1, Phase2), name multi-arm
variants after their specific configuration (e.g., Enabled<Variant>) rather than
using generic Enabled, and use zero-padded numbers (e.g., \_002, \_005, \_010)
instead of letter prefixes for numeric parameter arms.

**Why.** Top-level keys define the FieldTrial study name and must match
server-side Finch configurations and disambiguate concurrent platform trials. In
experiment groups, descriptive phase and variant names clarify what
configuration each arm exercises on waterfall test bots, and zero-padding
numeric arms preserves natural ascending order under lexicographical dashboard
sorting.

## Examples

Bad:

```cpp
// Study key with redundant Study suffix, phase descriptor, or missing platform qualifier for platform-specific trial:
"SkipMediaCodecReallocationStudy": [
  {"platforms": ["android"], "experiments": [{"name": "Enabled", "enable_features": ["SkipMediaCodecReallocation"]}]}
],
"AutofillAiM4": [
  {"platforms": ["android"], "experiments": [{"name": "Enabled", "enable_features": ["AutofillAiM4"]}]}
],
// Phased rollout using generic Enabled:
"EncryptBookmarks": [
  {"platforms": ["windows"], "experiments": [{"name": "Phase1", "params": {"stage": "phase1"}, "enable_features": ["EncryptBookmarks"]}]}
],
// Multi-arm trial mixing generic Enabled with specialized arm:
"EnableDrDc": [
  {
    "platforms": ["mac"],
    "experiments": [
      {"name": "Enabled", "enable_features": ["EnableDrDc", "GraphiteContextIsThreadSafe"]},
      {"name": "EnabledMultipleGraphiteContext", "enable_features": ["EnableDrDc"]}
    ]
  }
],
// Numeric parameter groups with arbitrary letter prefixes:
"MaxEventsStudy": [
  {
    "platforms": ["linux"],
    "experiments": [
      {"name": "MaxEvents_a_2", "params": {"max_events": "2"}},
      {"name": "MaxEvents_b_5", "params": {"max_events": "5"}},
      {"name": "MaxEvents_c_10", "params": {"max_events": "10"}}
    ]
  }
]
```

Good:

```cpp
// Single-feature study matches flag; platform-split/platform-specific trials include platform qualifiers:
"SkipMediaCodecReallocation": [
  {"platforms": ["android"], "experiments": [{"name": "Enabled", "enable_features": ["SkipMediaCodecReallocation"]}]}
],
"AutofillAiM4Android": [
  {"platforms": ["android"], "experiments": [{"name": "Enabled", "enable_features": ["AutofillAiM4"]}]}
],
"DelayMainJobStudy": [{"platforms": ["android"], "experiments": [{"name": "Enabled", "enable_features": ["DelayMainJob"]}]}],
"DelayMainJobStudyDesktop": [{"platforms": ["windows"], "experiments": [{"name": "Enabled", "enable_features": ["DelayMainJob"]}]}],
// Phased rollout using phase-specific group name:
"EncryptBookmarks": [
  {"platforms": ["windows"], "experiments": [{"name": "Phase1", "params": {"stage": "phase1"}, "enable_features": ["EncryptBookmarks"]}]}
],
// Multi-arm trial using symmetric, descriptive variant names:
"EnableDrDc": [
  {
    "platforms": ["mac"],
    "experiments": [
      {"name": "EnabledGraphiteContextThreadSafe", "enable_features": ["EnableDrDc", "GraphiteContextIsThreadSafe"]},
      {"name": "EnabledGraphiteContextPerThread", "enable_features": ["EnableDrDc"], "disable_features": ["GraphiteContextIsThreadSafe"]}
    ]
  }
],
// Numeric parameter groups using zero-padded numbers:
"MaxEventsStudy": [
  {
    "platforms": ["linux"],
    "experiments": [
      {"name": "MaxEvents_002", "params": {"max_events": "2"}},
      {"name": "MaxEvents_005", "params": {"max_events": "5"}},
      {"name": "MaxEvents_010", "params": {"max_events": "10"}}
    ]
  }
]
```
