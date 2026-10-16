---
name: general/include-dynamic-state-in-test-failure-messages
rubric: readability
severity: nit
---

**Rule.** When providing custom failure messages for test assertions or test
helper verifications (such as streaming with `<<` into `EXPECT_*`, `ASSERT_*`,
or `ADD_FAILURE()`), stream relevant dynamic runtime state (such as container
sizes, actual counts, remaining items, or received values) alongside static
descriptive text.

**Why.** Static failure messages only indicate that an invariant failed, leaving
developers guessing about the actual runtime state. Streaming dynamic details
like container sizes, counts, or values into assertion messages makes failure
output immediately actionable in test logs without requiring a debugger or test
re-runs.

## Examples

Bad:

```cpp
EXPECT_TRUE(values.empty())
    << "TestFuture destroyed with unconsumed values. This likely means "
       "that the test did not wait for all expected callbacks.";
```

Good:

```cpp
EXPECT_TRUE(values.empty())
    << "TestFuture(Mode::kQueue) destroyed with " << values.size()
    << " unconsumed values. This likely means that the test did not "
       "wait for all expected callbacks.";
```
