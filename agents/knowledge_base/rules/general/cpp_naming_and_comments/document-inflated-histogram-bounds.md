---
name: general/document-inflated-histogram-bounds
rubric: readability
severity: should_fix
---

**Rule.** When defining custom timing or count histograms for configured target
values or deterministic parameters, size the upper bound to the maximum expected
configuration limit to maximize bucket resolution. For elapsed execution times
where the upper bound is intentionally inflated significantly above nominal
durations (such as adding headroom for OS thread descheduling), document the
rationale in an explanatory comment.

**Why.** Inflating upper bounds for deterministic parameters wastes histogram
bucket resolution over an unreachable range. Conversely, elapsed execution times
require headroom to capture preemption or tail latency, and documenting that
rationale prevents maintainers from mistaking wide bounds for misconfigurations.

## Examples

Bad:

```cpp
// 1. Copying inflated 100ms bounds for a deterministic target capped at 10ms:
UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
    "Scheduling.MessagePumpDefault.BusyLoop.TargetDuration",
    max_busy_loop_time, base::Microseconds(1), base::Milliseconds(100), 50);

// 2. Missing explanation for inflated upper bound on elapsed execution time:
UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
    "Scheduling.MessagePumpDefault.BusyLoop.Duration.TaskArrived",
    busy_loop_duration, base::Microseconds(1), base::Milliseconds(100), 50);
```

Good:

```cpp
// 1. Sizing bound to the maximum valid configuration range to preserve resolution:
UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
    "Scheduling.MessagePumpDefault.BusyLoop.TargetDuration",
    max_busy_loop_time, base::Microseconds(1), base::Milliseconds(10), 50);

// 2. Documenting inflated headroom used to capture thread descheduling:
// The maximum busy loop time is much lower than 100ms, but set the
// histogram's upper bound to 100ms to capture thread descheduling latency.
UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
    "Scheduling.MessagePumpDefault.BusyLoop.Duration.TaskArrived",
    busy_loop_duration, base::Microseconds(1), base::Milliseconds(100), 50);
```
