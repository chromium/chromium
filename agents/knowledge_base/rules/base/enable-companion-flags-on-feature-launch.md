---
name: base/enable-companion-flags-on-feature-launch
rubric: correctness
severity: should_fix
---

**Rule.** When launching a feature by setting its `base::Feature` definition to
`base::FEATURE_ENABLED_BY_DEFAULT`, check the corresponding Finch or field trial
configuration and ensure all required companion, sub-feature, task observer, and
layer-specific hook flags are also switched to `FEATURE_ENABLED_BY_DEFAULT`. Do
not leave dependent companion flags disabled by default when the parent feature
launches.

**Why.** Features rolled out in field trials often coordinate multiple
`base::Feature` flags across subsystems or embedder layers. Enabling only the
primary feature flag leaves companion hooks disabled for standard clients
running without Finch overrides, creating behavioral divergence between
experimental and default-launched code paths.

## Examples

Bad:

```cpp
// Primary feature is enabled by default for launch:
BASE_FEATURE(kPartitionAllocSchedulerLoopQuarantine,
             base::FEATURE_ENABLED_BY_DEFAULT);

// In content/common/features.cc, companion hook flag remains disabled:
BASE_FEATURE(
    kPartitionAllocSchedulerLoopQuarantineTaskObserverForBrowserUIThread,
    base::FEATURE_DISABLED_BY_DEFAULT);
```

Good:

```cpp
// Primary feature and all companion flags are enabled by default for launch:
BASE_FEATURE(kPartitionAllocSchedulerLoopQuarantine,
             base::FEATURE_ENABLED_BY_DEFAULT);

// In content/common/features.cc, companion hook flag is also enabled:
BASE_FEATURE(
    kPartitionAllocSchedulerLoopQuarantineTaskObserverForBrowserUIThread,
    base::FEATURE_ENABLED_BY_DEFAULT);
```
