---
name: general/declare-inline-methods-constexpr-on-value-classes
rubric: readability
severity: should_fix
---

**Rule.** When adding or updating inline member methods or arithmetic operators
on constexpr-capable value classes, wrapper types, or vocabulary classes (such
as base::TimeBase, base::TimeDelta, or geometric and numeric types), declare
them constexpr whenever their implementation allows compile-time evaluation.
Ensure all non-virtual inline query, conversion, and arithmetic methods maintain
API consistency with the rest of the class by supporting compile-time
evaluation.

**Why.** Value types and arithmetic wrapper classes in Chromium are designed to
support compile-time evaluation across constants, static assertions, and
template parameters. Omitting constexpr on inline member methods creates API
inconsistencies and prevents callers from using those methods in constexpr
contexts.

## Examples

Bad:

```cpp
// In base::TimeBase or another constexpr-friendly value class:
TimeClass SnappedToNextTick(TimeClass tick_phase,
                            TimeDelta tick_interval) const {
  TimeDelta interval_offset = (tick_phase - *this) % tick_interval;
  if (!interval_offset.is_zero() && tick_phase < *this) {
    interval_offset += tick_interval;
  }
  return *this + interval_offset;
}
```

Good:

```cpp
// In base::TimeBase or another constexpr-friendly value class:
constexpr TimeClass SnappedToNextTick(TimeClass tick_phase,
                                      TimeDelta tick_interval) const {
  TimeDelta interval_offset = (tick_phase - *this) % tick_interval;
  if (!interval_offset.is_zero() && tick_phase < *this) {
    interval_offset += tick_interval;
  }
  return *this + interval_offset;
}
```
