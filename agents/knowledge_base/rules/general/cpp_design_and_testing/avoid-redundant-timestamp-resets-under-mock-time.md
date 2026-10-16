---
name: general/avoid-redundant-timestamp-resets-under-mock-time
rubric: readability
severity: nit
---

**Rule.** Do not add redundant timestamp or clock resets across test runs when
relying on TaskEnvironment mock time (TimeSource::MOCK_TIME). Because the mock
clock initializes from the current live system time rather than a fixed epoch at
test startup, consecutive test executions naturally observe advancing TimeTicks
values without manual timestamp resets.

**Why.** Under base::test::TaskEnvironment with TimeSource::MOCK_TIME, the mock
clock starts at the real system time and advances monotonically across
successive tests running in the same process. Resetting timestamp tracking
fields to a sentinel (such as base::TimeTicks()) between tests or in
Reset...ForTesting() helpers is redundant.

## Examples

Bad:

```cpp
void RunLoop::ResetSpinDetectorForTesting() {
  g_spin_detector.consecutive_hollow_calls = 0;
  // Redundant: TaskEnvironment MOCK_TIME seeds from the live clock.
  g_spin_detector.last_time = base::TimeTicks();
}
```

Good:

```cpp
void RunLoop::ResetSpinDetectorForTesting() {
  DCHECK_EQ(g_spin_detector.observer_depth, 0);
  g_spin_detector.consecutive_hollow_calls = 0;
}
```
