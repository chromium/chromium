---
name: testing/consolidate-multi-feature-field-trials-in-testing-config
rubric: correctness
severity: should_fix
---

**Rule.** In testing/variations/fieldtrial_testing_config.json, align top-level
study entries with their server-side FieldTrial definitions. Consolidate all
feature flags controlled by a single multi-feature trial into one study's
enable_features list, and bundle mutually exclusive experiment arms into that
study's experiments array. Conversely, keep distinct FieldTrial studies (such as
platform-split or variant-specific trials) separated, defining experiment groups
and parameters strictly under their respective study entries rather than mixing
them into unrelated studies.

**Why.** Top-level keys in fieldtrial_testing_config.json map directly to client
and server-side FieldTrial study names. Splitting a single multi-feature trial
across multiple studies breaks consistency checks and violates PRESUBMIT.py
(CheckDuplicatedFeatures) on overlapping platforms. Conversely, adding
experiment groups or parameters meant for one trial into an unrelated study
conflates independent experiment configurations, leading to invalid bot
configurations or dropped test coverage.

## Examples

Bad:

```cpp
// Anti-pattern 1: Separate studies for multi-feature trial
"AddAutomaticWithDohFallbackMode": [
  {"platforms": ["linux", "mac", "windows"], "experiments": [{"name": "Enabled", "enable_features": ["AddAutomaticWithDohFallbackMode"]}]}
],
"ProbeSecureDnsCanaryDomain": [
  {"platforms": ["linux", "mac", "windows"], "experiments": [{"name": "Enabled", "enable_features": ["ProbeSecureDnsCanaryDomain"]}]}
],
// Anti-pattern 2: Adding distinct trial variant experiment into an unrelated study
"PartitionAllocWithAdvancedChecks": [
  {
    "platforms": ["linux", "mac", "windows"],
    "experiments": [
      {"name": "enabled_512K_20260203", "enable_features": ["PartitionAllocAdvancedChecks"]},
      {"name": "enabled_amsc_all_processes", "params": {"PartitionAllocSchedulerLoopQuarantineConfig": "{\"*\":{\"amsc\":...}}"}}
    ]
  }
]
```

Good:

```cpp
// Pattern 1: Multi-feature trial grouped under one study
"ForcedDohFallbackWithCanaryDomain": [
  {
    "platforms": ["linux", "mac", "windows"],
    "experiments": [{
      "name": "Enabled",
      "params": {"canary_domain_host": "use-application-dns.net"},
      "enable_features": ["AddAutomaticWithDohFallbackMode", "ProbeSecureDnsCanaryDomain"]
    }]
  }
],
// Pattern 2: Distinct trial variants separated into their respective study entries
"PartitionAllocWithAdvancedChecks": [
  {"platforms": ["linux", "mac", "windows"], "experiments": [{"name": "enabled_512K_20260203", "enable_features": ["PartitionAllocAdvancedChecks"]}]}
],
"PartitionAllocWithAdvancedChecksAMSC": [
  {"platforms": ["android", "chromeos", "ios"], "experiments": [{"name": "enabled_amsc_all_processes", "params": {"PartitionAllocSchedulerLoopQuarantineConfig": "{\"*\":{\"amsc\":...}}"}}]}
]
```
