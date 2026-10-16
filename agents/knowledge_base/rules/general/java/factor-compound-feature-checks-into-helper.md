---
name: general/factor-compound-feature-checks-into-helper
rubric: design
severity: should_fix
---

**Rule.** Factor out compound feature, capability, or environment checks (such
as those combining build configurations, system delegates, and feature flags)
into a single authoritative helper method. Do not re-implement, inline, or
duplicate partial subsets of the condition across multiple call sites or across
abstraction layers.

**Why.** Duplicating partial subsets of a compound enablement condition across
call sites risks omitting required build flags, delegate checks, or feature
toggles, leading to behavioral discrepancies and broken layer encapsulation.
Centralizing the full condition in an authoritative helper guarantees consistent
evaluation and allows higher layers to pass resolved state down cleanly.

## Examples

Bad:

```cpp
// Lower-level component duplicates an incomplete subset of the feature condition:
int maxProcessLimit = MAX_VARIABLE_ALLOCATED;
AconfigFlaggedApiDelegate delegate = AconfigFlaggedApiDelegate.getInstance();
if (delegate != null && delegate.areNativeOnlyServicesEnabled()) {
  maxProcessLimit = MAX_PROCESSES_WITH_NATIVE_ZYGOTE;
}
```

Good:

```cpp
// Authoritative helper encapsulates the complete multi-part condition:
public static boolean isNativeSandboxedServiceEnabled() {
  AconfigFlaggedApiDelegate delegate = AconfigFlaggedApiDelegate.getInstance();
  return delegate != null
      && delegate.areNativeOnlyServicesEnabled()
      && BuildConfig.JAVALESS_RENDERERS_AVAILABLE
      && !BuildConfig.IS_INCREMENTAL_INSTALL
      && JavalessRenderersFeatureList.isEnabled();
}

// Callers invoke the helper once and pass the resolved parameter down:
int maxProcessLimit =
    ChildProcessCreationParamsImpl.isNativeSandboxedServiceEnabled()
        ? ChildConnectionAllocator.MAX_PROCESSES_WITH_NATIVE_ZYGOTE
        : ChildConnectionAllocator.MAX_VARIABLE_ALLOCATED;
connectionAllocator = ChildConnectionAllocator.createVariableSize(..., maxProcessLimit);
```
