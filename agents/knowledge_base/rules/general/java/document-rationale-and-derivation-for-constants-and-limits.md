---
name: general/document-rationale-and-derivation-for-constants-and-limits
rubric: readability
severity: should_fix
---

**Rule.** When defining non-obvious constants, synthetic IDs, retry limits,
delays, timeouts, or heuristic tuning thresholds (including query and pipeline
stage thresholds), add a code comment explaining their rationale, derivation,
default assumptions, collision-avoidance constraints, or the precedent from
which the values were chosen. When specifying stage- or context-specific
threshold deviations from a shared default (such as timing margins relative to
vsync), document why the custom value is appropriate.

**Why.** Arbitrary, unmotivated, or unexplained constants, IDs, and heuristic
thresholds look like magic numbers or protocol constants, making maintenance
difficult and prompting reviewer confusion. Documenting their derivation,
baseline empirical data, hardware/timing invariants, collision avoidance, or
referenced precedent clarifies design intent and prevents accidental regressions
or collisions.

## Examples

Bad:

```cpp
// C++ constants:
constexpr int kMaxTries = 25;
constexpr base::TimeDelta kDelay = base::Milliseconds(50);

// Java synthetic IDs / constants:
private static final long TRACE_ID_ON_UI_THREAD = 1000;
private static final long TRACE_ID_ON_LAUNCHER_THREAD = 1001;

// SQL / query heuristic thresholds:
_chrome_scroll_jank_tag_long_stage!(viz_to_gpu_delay_dur, 'long_viz_to_gpu', 6)
```

Good:

```cpp
// C++ constants:
// This retry mechanic is inspired by the proven OOM handling logic found in
// components/memory_pressure/system_memory_pressure_evaluator_win.cc.
constexpr int kMaxTries = 25;
constexpr base::TimeDelta kDelay = base::Milliseconds(50);

// Java synthetic IDs / constants:
// Arbitrary trace ID offset chosen to not overlap the process PID range.
private static final long TRACE_ID_ON_UI_THREAD = (1 << 22) + 1;
private static final long TRACE_ID_ON_LAUNCHER_THREAD = TRACE_ID_ON_UI_THREAD + 1;

// SQL / query heuristic thresholds:
-- Lower threshold (6ms) because IPC hop to GPU should be fast.
_chrome_scroll_jank_tag_long_stage!(viz_to_gpu_delay_dur, 'long_viz_to_gpu', 6)
```
