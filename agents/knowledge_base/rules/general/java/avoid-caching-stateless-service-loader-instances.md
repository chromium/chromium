---
name: general/avoid-caching-stateless-service-loader-instances
rubric: readability
severity: should_fix
---

**Rule.** When obtaining stateless service or delegate instances loaded via
ServiceLoaderUtil, use the class's dedicated accessor method (e.g.,
AconfigFlaggedApiDelegate.getInstance()) rather than calling
ServiceLoaderUtil.maybeCreate() directly. In accessor implementations and at
call sites, avoid caching returned stateless instances in static fields, member
variables, or holder classes; query the instance on demand so R8 can inline
calls and eliminate allocations. When implementing test override methods,
delegate directly to ServiceLoaderUtil.setInstanceForTesting(Class<T>, T)
instead of maintaining custom static test-instance fields or manual
ResettersForTesting registrations.

**Why.** Dedicated accessors encapsulate lookup logic and avoid direct
ServiceLoaderUtil dependencies across call sites, while non-cached on-demand
queries allow R8 to inline lookups and eliminate allocations for stateless
services in release builds. Delegating test overrides to
ServiceLoaderUtil.setInstanceForTesting centralizes test double tracking and
automatically registers cleanup via ResettersForTesting, eliminating redundant
state-holding boilerplate.

## Examples

Bad:

```cpp
// Anti-pattern 1: Caching stateless service in static holder
class PrivateHolder {
    private static final @Nullable AconfigFlaggedApiDelegate sInstance =
            ServiceLoaderUtil.maybeCreate(AconfigFlaggedApiDelegate.class);
    private static @Nullable AconfigFlaggedApiDelegate sInstanceForTesting;
}

// Anti-pattern 2: Manual static test instance field and resetter
static void setInstanceForTesting(AconfigFlaggedApiDelegate testInstance) {
    PrivateHolder.sInstanceForTesting = testInstance;
    ResettersForTesting.register(() -> PrivateHolder.sInstanceForTesting = null);
}

// Anti-pattern 3: Calling ServiceLoaderUtil directly at call sites
final AconfigFlaggedApiDelegate delegate =
        ServiceLoaderUtil.maybeCreate(AconfigFlaggedApiDelegate.class);
```

Good:

```cpp
// Accessor retrieves on demand without caching
static @Nullable AconfigFlaggedApiDelegate getInstance() {
    return ServiceLoaderUtil.maybeCreate(AconfigFlaggedApiDelegate.class);
}

// Delegate test overrides directly to ServiceLoaderUtil
static void setInstanceForTesting(AconfigFlaggedApiDelegate testInstance) {
    ServiceLoaderUtil.setInstanceForTesting(AconfigFlaggedApiDelegate.class, testInstance);
}

// Callers use dedicated accessor on demand
final AconfigFlaggedApiDelegate delegate = AconfigFlaggedApiDelegate.getInstance();
```
