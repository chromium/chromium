---
name: general/avoid-redundant-test-override-checks-in-getters
rubric: readability
severity: nit
---

**Rule.** In Android Java singletons where instance caching is disabled in test
environments (e.g., returning a new instance on each query when
BuildConfig.IS_FOR_TEST is true) and instance construction already incorporates
static test overrides, do not duplicate checks for static test override
variables in public getter methods. Query the singleton instance directly via
getInstance().

**Why.** When instance construction or data snapshotting already evaluates
static test override fields and getInstance() creates a fresh instance per call
in test environments, checking the static override variable in public accessors
adds redundant branching and dead code. Relying on getInstance() ensures a
single source of truth for both production and test logic.

## Examples

Bad:

```cpp
public static boolean isDesktop() {
    if (sIsDesktopForTesting != null) {
        return sIsDesktopForTesting;
    }
    return getInstance().mIDeviceInfo.isDesktop;
}
```

Good:

```cpp
public static boolean isDesktop() {
    return getInstance().mIDeviceInfo.isDesktop;
}
```
