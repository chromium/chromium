---
name: general/use-consistent-time-units-in-test-advancements
rubric: readability
severity: nit
---

**Rule.** In unit tests that incrementally advance mock time (such as via
FastForwardBy or AdvanceClock), express delay and timeout constants using the
same time unit (e.g., base::Seconds(60)) as the step increments rather than
mixed units (e.g., base::Minutes(1)).

**Why.** Using the same time unit across delay definitions and step-by-step mock
time advancements reduces cognitive load and simplifies verifying that step
increments sum up to the overall expected duration.

## Examples

Bad:

```cpp
// Bad: kDelay is defined in minutes while step increments use seconds.
constexpr auto kDelay = base::Minutes(1);
wall_clock_timer.Start(FROM_HERE, clock_.Now() + kDelay, callback.Get());

task_runner->FastForwardBy(base::Seconds(30));
// ...
task_runner->FastForwardBy(base::Seconds(30));
```

Good:

```cpp
// Good: both constant and step increments use the same time unit (seconds).
constexpr auto kDelay = base::Seconds(60);
wall_clock_timer.Start(FROM_HERE, clock_.Now() + kDelay, callback.Get());

task_runner->FastForwardBy(base::Seconds(30));
// ...
task_runner->FastForwardBy(base::Seconds(30));
```
