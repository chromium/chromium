---
name: general/use-parameterized-setter-for-boolean-test-overrides
rubric: readability
severity: should_fix
---

**Rule.** When defining test override methods for boolean state or flags, expose
a single parameterized setter (such as `setFooForTesting(boolean value)`) rather
than declaring separate parameterless methods for positive and negative values
(such as `setFooForTesting()` and `setNonFooForTesting()`). Register state
restoration inside the parameterized setter via
`ResettersForTesting.register(...)`.

**Why.** A single parameterized setter prevents method proliferation, provides a
uniform API consistent with Chromium conventions, and centralizes state mutation
and ResettersForTesting cleanup in one place.

## Examples

Bad:

```cpp
// Anti-pattern: Separate parameterless methods for positive and negative values
public static void setIsXrForTesting() {
    sIsXrForTesting = true;
    ResettersForTesting.register(() -> sIsXrForTesting = null);
}

public static void setNonXrForTesting() {
    sIsXrForTesting = false;
    ResettersForTesting.register(() -> sIsXrForTesting = null);
}
```

Good:

```cpp
// Idiomatic: A single parameterized setter handling both true and false values
public static void setIsXrForTesting(boolean value) {
    sIsXrForTesting = value;
    ResettersForTesting.register(() -> sIsXrForTesting = null);
}
```
