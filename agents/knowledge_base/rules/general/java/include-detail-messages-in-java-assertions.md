---
name: general/include-detail-messages-in-java-assertions
rubric: readability
severity: nit
---

**Rule.** In Java assertions, provide an informative detail message
(`assert <condition> : <message>;`) for state invariants, preconditions, and
lifecycle checks so that assertion failures provide clear diagnostic context.

**Why.** Java assertions in Chromium are enabled in debug builds and reported
via a silent assertion reporter on Canary, with R8 stripping assertion
expressions and messages in release builds. Bare assertions throw an
AssertionError without detail, making test failure triage and Canary crash
report analysis significantly harder.

## Examples

Bad:

```cpp
assert sLauncherHandler == null;
```

Good:

```cpp
assert sLauncherHandler == null : "ScopedBatchUpdate was already activated.";
```
