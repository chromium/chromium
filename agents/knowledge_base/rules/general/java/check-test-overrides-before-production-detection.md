---
name: general/check-test-overrides-before-production-detection
rubric: correctness
severity: should_fix
---

**Rule.** When initializing internal state, member variables, or cached
properties that support a static test override (such as sFooForTesting), check
whether the test override is non-null before executing production detection
logic or querying external dependencies (such as CommandLine.getInstance(),
PackageManager, or system services). Ensure downstream calculations and cached
fields use the overridden value directly rather than evaluating uninitialized
services.

**Why.** In unit test or Robolectric environments, external services, system
feature queries, and command-line singletons like CommandLine.getInstance() may
not be initialized, causing unexpected exceptions, assertion failures, or
incorrect detection state. Checking the test override first bypasses
uninitialized dependencies and ensures internal state and native synchronization
respect test fixtures.

## Examples

Bad:

```cpp
// Anti-pattern: Queries CommandLine and PackageManager without checking test override first
mDeviceInfo.isDesktop =
        (BuildConfig.IS_DESKTOP_ANDROID && pm.hasSystemFeature(PackageManager.FEATURE_PC))
                || CommandLine.getInstance().hasSwitch(BaseSwitches.FORCE_DESKTOP_ANDROID);
```

Good:

```cpp
// Idiomatic: Evaluates test override before querying external dependencies
mDeviceInfo.isDesktop =
        (sIsDesktopForTesting != null)
                ? sIsDesktopForTesting
                : ((BuildConfig.IS_DESKTOP_ANDROID
                                && pm.hasSystemFeature(PackageManager.FEATURE_PC))
                        || CommandLine.getInstance()
                                .hasSwitch(BaseSwitches.FORCE_DESKTOP_ANDROID));
```
