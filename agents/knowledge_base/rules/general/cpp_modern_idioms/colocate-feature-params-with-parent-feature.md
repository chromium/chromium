---
name: general/colocate-feature-params-with-parent-feature
rubric: readability
severity: should_fix
---

**Rule.** Place BASE_FEATURE_PARAM and BASE_FEATURE_ENUM_PARAM definitions (and
their BASE_DECLARE_FEATURE_PARAM header declarations) immediately below the
associated BASE_FEATURE or BASE_DECLARE_FEATURE macro they configure rather than
separating them or placing them at the bottom of the file.

**Why.** Colocating feature parameter declarations and definitions directly
beneath their parent feature keeps related feature configurations together and
makes available parameters readily discoverable.

## Examples

Bad:

```cpp
BASE_FEATURE(kPartitionAllocFreeWithSize, FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID) && defined(ARCH_CPU_ARM64)
BASE_FEATURE(kPartitionAllocLockTuneSpin, FEATURE_DISABLED_BY_DEFAULT);
// ...
#endif

BASE_FEATURE_PARAM(bool,
                   kPartitionAllocStrictFreeSizeCheck,
                   &kPartitionAllocFreeWithSize,
                   "strict-free-size-check",
                   true);
```

Good:

```cpp
BASE_FEATURE(kPartitionAllocFreeWithSize, FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(bool,
                   kPartitionAllocStrictFreeSizeCheck,
                   &kPartitionAllocFreeWithSize,
                   "strict-free-size-check",
                   true);

#if BUILDFLAG(IS_ANDROID) && defined(ARCH_CPU_ARM64)
BASE_FEATURE(kPartitionAllocLockTuneSpin, FEATURE_DISABLED_BY_DEFAULT);
```
