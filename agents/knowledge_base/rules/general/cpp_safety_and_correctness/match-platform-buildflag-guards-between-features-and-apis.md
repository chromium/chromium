---
name: general/match-platform-buildflag-guards-between-features-and-apis
rubric: correctness
severity: should_fix
---

**Rule.** Keep platform buildflag guards (`#if BUILDFLAG(...)`) consistent
between `base::Feature` declarations, API method declarations, and their
implementations. In source files shared across multiple platforms (such as POSIX
and Fuchsia sharing headers or `*_posix.cc` implementations), do not expand a
new method's platform guard to encompass all shared platforms if the underlying
feature is only defined for a specific platform.

**Why.** Inconsistent platform guards expose API declarations or methods on
platforms where the backing feature flag or underlying implementation is
undeclared or unsupported, leading to compilation failures, undefined symbols,
or dead interfaces on secondary platforms.

## Examples

Bad:

```cpp
// base/features.h
#if BUILDFLAG(IS_POSIX)
BASE_EXPORT BASE_DECLARE_FEATURE(kBaseLockTrySpin);
#endif

// base/synchronization/lock_impl.h
#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  static void SetTrySpinCount(int spin_count);
#endif
```

Good:

```cpp
// base/features.h
#if BUILDFLAG(IS_POSIX)
BASE_EXPORT BASE_DECLARE_FEATURE(kBaseLockTrySpin);
#endif

// base/synchronization/lock_impl.h
#if BUILDFLAG(IS_POSIX)
  static void SetTrySpinCount(int spin_count);
#endif
```
