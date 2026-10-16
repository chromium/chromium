---
name: general/explain-cross-platform-rollout-in-commit-message
rubric: readability
severity: should_fix
---

**Rule.** When authoring commit message descriptions for platform changes or
rollout configurations (such as `fieldtrial_testing_config.json` or feature
flags), provide an explanatory body specifying concrete modifications—including
newly targeted platforms (e.g., `android_webview`), split or restructured
experiments, and accurate platform lists without naming un-enabled platforms.
When a change's motivation targets a specific platform but enables
cross-platform rollout, explain the rationale for the wider rollout in the
commit description.

**Why.** Commit descriptions become permanent repository history relied upon
during regression triage, bisection, cherry-picks, and cross-platform test
evaluation. A bare subject line or inaccurate platform list obscures test impact
and misleads engineers debugging platform-specific failures.

## Examples

Bad:

```cpp
// Bare subject line omitting newly added platforms or split experiments:
PrefetchPrerenderInteg: Add fieldtrial_testing_config.json

Bug: none

// Listing platforms not actually enabled by the configuration:
This configuration enables the "ProbeSecureDnsCanaryDomain" feature and sets
the "canary_domain_host" parameter to "use-application-dns.net" for Android,
Linux, Mac, and Windows platforms.

// Omitting rollout rationale when enabling cross-platform:
CL title: [Sync] Bypass scheduler for sync invalidations on Android
CL description: This change improves receiving flow of high-priority sync
invalidations.
// In fieldtrial_testing_config.json, "platforms": ["android", "ios", "linux", "mac", "windows"]
```

Good:

```cpp
// Documenting split experiments and newly added platforms:
PrefetchPrerenderInteg: Add fieldtrial_testing_config.json

This CL splits the feature flag `kPrefetchPrerenderIntegration` from
the experiment `DsePreload2` and adds platform android_webview.

Bug: 496807876

// Accurately listing only the platforms actually enabled:
This configuration enables the "ProbeSecureDnsCanaryDomain" feature and sets
the "canary_domain_host" parameter to "use-application-dns.net" for Linux,
Mac, and Windows platforms.

// Documenting cross-platform rollout rationale:
CL title: [Sync] Bypass scheduler for sync invalidations on Android
CL description: This change improves receiving flow of high-priority sync
invalidations.
Note: Effective on Android only, but rolled out on all platforms in
fieldtrial_testing_config.json to maintain a single cross-platform codepath.
```
