---
name: general/use-test-timeouts-instead-of-hardcoded-durations
rubric: readability
severity: should_fix
---

**Rule.** In unit and integration tests, use TestTimeouts methods (such as
TestTimeouts::tiny_timeout(), TestTimeouts::action_timeout(), or
TestTimeouts::action_max_timeout()) rather than hardcoding fixed durations
(e.g., base::Milliseconds(10) or base::Seconds(5)) for timeouts, event waits, or
action delays. Select tiny_timeout() for near-instantaneous polling or delays,
action_timeout() for standard asynchronous completions, and action_max_timeout()
for longer success conditions.

**Why.** Hardcoded durations frequently cause test flakiness and false-positive
timeouts on slower platforms, virtual machines, heavily loaded trybots, and
instrumented builds (such as ASan, MSan, or TSan). TestTimeouts scales duration
values automatically based on the build configuration, execution environment,
and command-line test multiplier flags.

## Examples

Bad:

```cpp
// Hardcoded durations cause flakiness on slow trybots and sanitizers.
base::TimeDelta short_delay = base::Milliseconds(10);
EXPECT_TRUE(event.TimedWait(short_delay));

child_process.WaitForExitWithTimeout(base::Seconds(5), &exit_code);
```

Good:

```cpp
// TestTimeouts scales automatically with platform and test runner multipliers.
const base::TimeDelta short_delay = TestTimeouts::tiny_timeout();
EXPECT_TRUE(event.TimedWait(short_delay));

child_process.WaitForExitWithTimeout(TestTimeouts::action_timeout(), &exit_code);
```
