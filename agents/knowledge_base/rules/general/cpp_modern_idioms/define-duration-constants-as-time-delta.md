---
name: general/define-duration-constants-as-time-delta
rubric: readability
severity: should_fix
---

**Rule.** Define time duration and timeout constants directly as
`constexpr base::TimeDelta` (using helpers such as `base::Milliseconds()`,
`base::Seconds()`, or `base::Minutes()`) rather than as raw integer or
floating-point constants with unit suffixes (e.g., `kDelayMs = 50`).

**Why.** Instantiating `base::TimeDelta` directly at the constant definition
provides compile-time type safety, prevents unit mismatches and redundant
conversions across call sites, and carries no runtime overhead because
`base::TimeDelta` is `constexpr`.

## Examples

Bad:

```cpp
// Bad: raw integer constant requires manual unit conversion at call sites.
constexpr int kPollingIntervalMs = 50;
base::PlatformThread::Sleep(base::Milliseconds(kPollingIntervalMs));
```

Good:

```cpp
// Good: strongly typed TimeDelta constant is safe and self-documenting.
constexpr base::TimeDelta kPollingInterval = base::Milliseconds(50);
base::PlatformThread::Sleep(kPollingInterval);
```
