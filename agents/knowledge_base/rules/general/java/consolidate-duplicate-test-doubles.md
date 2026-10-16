---
name: general/consolidate-duplicate-test-doubles
rubric: design
severity: should_fix
---

**Rule.** Do not duplicate fake, mock, or stub implementations of the same
interface across different test files or test suites. Consolidate shared test
doubles into a common test-support target, fixture, or helper class so multiple
test files reuse a single canonical implementation.

**Why.** Duplicating test doubles across multiple test files leads to code
duplication, increased maintenance churn, and divergent mock behaviors when
interface contracts evolve. Consolidating test doubles in shared test support
ensures consistent testing semantics and reduces maintenance overhead.

## Examples

Bad:

```cpp
// In LegacyChildServiceConnectionControllerTest.java:
private static class TestChildServiceConnection implements ChildServiceConnection {
    private boolean mBound;
    private final boolean mBindSuccess;
    // ...
}
// Duplicating ChildServiceConnection test doubles already defined in other test files.
```

Good:

```cpp
// In shared test support (e.g. TestChildServiceConnection.java in test_support target):
public class TestChildServiceConnection implements ChildServiceConnection {
    private boolean mBound;
    private final boolean mBindSuccess;
    // ...
}
// Reused across LegacyChildServiceConnectionControllerTest and other test suites.
```
