---
name: general/prefix-fallible-actions-with-try
rubric: readability
severity: nit
---

**Rule.** Name methods that conditionally perform an action or lifecycle
operation based on internal precondition checks (such as feature flags, resource
availability, or state validation) with a 'try...' (Java) or 'Try...' (C++)
prefix when returning a boolean indicating success, rather than using an
unconditional imperative verb (e.g., 'activate()', 'start()').

**Why.** An imperative method name implies an unconditional command that callers
expect to either succeed or assert/throw on failure. Prefixing fallible
operations with 'try' or 'Try' makes the conditional contract explicit,
signaling to callers that the method returns a success status and eliminating
redundant external precondition checks.

## Examples

Bad:

```cpp
// In ScopedServiceBindingBatch:
public static boolean activate(Handler launcherHandler) {
    if (!isFeatureEnabled()) {
        return false;
    }
    ...
    return true;
}

// Caller:
if (ScopedServiceBindingBatch.isFeatureEnabled()) {
    boolean activated = ScopedServiceBindingBatch.activate(launcherHandler);
}
```

Good:

```cpp
// In ScopedServiceBindingBatch:
public static boolean tryActivate(Handler launcherHandler) {
    if (!isFeatureEnabled()) {
        return false;
    }
    ...
    return true;
}

// Caller:
boolean activated = ScopedServiceBindingBatch.tryActivate(launcherHandler);
```
