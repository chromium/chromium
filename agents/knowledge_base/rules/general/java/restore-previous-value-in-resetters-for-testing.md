---
name: general/restore-previous-value-in-resetters-for-testing
rubric: readability
severity: should_fix
---

**Rule.** In Java static test override setters (methods with a \*ForTesting
suffix mutating static state), register a reset callback using
ResettersForTesting.register(...) to ensure state mutations are automatically
cleaned up between tests. When mutating static state that may hold pre-existing
values or be overridden across nested test scopes, capture the field's previous
value into a local variable before updating and restore that previous value in
the lambda rather than unconditionally resetting to null or default.

**Why.** Chromium Android tests (both Robolectric unit tests and batched
on-device instrumentation tests) share process state across test methods.
Mutating static state without registering a resetter leaks state into subsequent
tests, causing test pollution and flakiness. Capturing and restoring the
previous value ensures reliable teardown across nested fixtures or sequential
overrides without destroying pre-existing configuration.

## Examples

Bad:

```cpp
// Anti-pattern 1: Mutating static state without registering a resetter
public static void setIsXrForTesting(boolean value) {
    sIsXrForTesting = value;
}

// Anti-pattern 2: Unconditionally resetting to null or default wipes out prior state
public static void setAmountOfPhysicalMemoryKbForTesting(int value) {
    sAmountOfPhysicalMemoryKB = value;
    ResettersForTesting.register(() -> sAmountOfPhysicalMemoryKB = null);
}
```

Good:

```cpp
// Idiomatic: Register teardown with ResettersForTesting, capturing and restoring previous value
public static void setIsXrForTesting(boolean value) {
    Boolean prev = sIsXrForTesting;
    sIsXrForTesting = value;
    ResettersForTesting.register(() -> sIsXrForTesting = prev);
}

public static void setAmountOfPhysicalMemoryKbForTesting(int value) {
    Integer prev = sAmountOfPhysicalMemoryKB;
    sAmountOfPhysicalMemoryKB = value;
    ResettersForTesting.register(() -> sAmountOfPhysicalMemoryKB = prev);
}
```
