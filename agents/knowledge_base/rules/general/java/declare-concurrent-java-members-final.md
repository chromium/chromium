---
name: general/declare-concurrent-java-members-final
rubric: safety
severity: should_fix
---

**Rule.** Declare Java member fields as final when their methods or state can be
read concurrently from threads outside the owning thread. Rather than
re-instantiating or reassigning non-final member fields during lifecycle
transitions (such as service fallback or rebinding), encapsulate internal state
transitions and reconnection within the member component itself.

**Why.** In Java, reading a non-final member field across threads without
synchronization while the owning thread reassigns it creates a data race that
can expose uninitialized state or cause NullPointerExceptions. Declaring the
member final guarantees safe publication under the Java memory model and ensures
the reference remains valid and immutable throughout the enclosing object's
lifetime.

## Examples

Bad:

```cpp
// Non-final field reassigned during fallback on launcher thread:
private ChildServiceConnectionController mConnectionController;

// Called from background thread:
public @ChildBindingState int bindingStateCurrent() {
    return mConnectionController.getBindingState();
}

private void fallbackService() {
    mConnectionController.retire();
    mConnectionController = new LegacyChildServiceConnectionController(...);
}
```

Good:

```cpp
// Final field safely published and immutable across threads:
private final ChildServiceConnectionController mConnectionController;

// Called from background thread:
public @ChildBindingState int bindingStateCurrent() {
    return mConnectionController.getBindingState();
}

private void fallbackService() {
    // Encapsulate reconnection and fallback state inside the controller:
    mConnectionController.replaceService(bindIntent);
}
```
