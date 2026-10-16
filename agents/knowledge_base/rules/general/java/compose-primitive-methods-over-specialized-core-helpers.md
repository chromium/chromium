---
name: general/compose-primitive-methods-over-specialized-core-helpers
rubric: design
severity: should_fix
---

**Rule.** Avoid expanding a core or low-level class interface with specialized
helper or state-resetting methods when callers can achieve the desired state
transition by composing existing primitive methods. Callers with domain-specific
knowledge of the object's current state should invoke primitive operations
directly.

**Why.** Adding specialized compound methods to foundational classes bloats
interfaces, couples core abstractions to specific caller lifecycles, and often
requires fragile assertions or artificial state resets. Composing existing
primitives at call sites preserves clean, minimal interfaces and prevents
redundant state-management paths.

## Examples

Bad:

```cpp
// BAD: Adding a specialized compound/resetting method to a core abstraction.
public class ChildProcessConnection {
    public void initializeEffectiveBindingState(@ChildBindingState int initialBindingState) {
        assert isRunningOnLauncherThread();
        mStrongBindingCount = 0;
        mVisibleBindingCount = 0;
        if (initialBindingState == ChildBindingState.STRONG) {
            mStrongBindingCount++;
        }
        mConnectionController.setEffectiveBindingState(initialBindingState);
    }
}
```

Good:

```cpp
// GOOD: Caller composes existing primitive methods using its domain context.
if (connection != null && initialBindingState != ChildBindingState.VISIBLE) {
    if (initialBindingState == ChildBindingState.STRONG) {
        connection.addStrongBinding();
    } else if (initialBindingState == ChildBindingState.NOT_PERCEPTIBLE) {
        connection.addNotPerceptibleBinding();
    }
    // Remove the spare connection's original visible binding now that new bindings are added.
    connection.removeVisibleBinding();
}
```
