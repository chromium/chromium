---
name: general/name-constants-after-conditions-not-relative-magnitude
rubric: readability
severity: nit
---

**Rule.** When defining variant constants, configuration limits, or capacity
thresholds, name them after the specific condition, feature, or operational mode
in which they apply (e.g., kMaxProcessesWithNativeZygote or
MAX_PROCESSES_WITH_NATIVE_ZYGOTE). Do not use vague relative qualifiers
describing magnitude (such as HIGH, LOW, LARGE, or SMALL) that obscure why an
alternative limit exists.

**Why.** Relative qualifiers like HIGH, LOW, or LARGE describe magnitude rather
than the conditions or modes under which a value is used. Naming constants after
the specific feature, condition, or operational mode makes call sites
self-documenting and explains why an alternative limit is chosen.

## Examples

Bad:

```cpp
// Higher max number of allocations allowed, for when Native Zygote is used.
private static final int MAX_PROCESSES_LIMIT_HIGH = 150;

// In C++:
// Fallback timeout when running on slow bots.
constexpr base::TimeDelta kTimeoutHigh = base::Seconds(30);
```

Good:

```cpp
// Higher max number of allocations allowed, for when Native Zygote is used.
private static final int MAX_PROCESSES_WITH_NATIVE_ZYGOTE = 150;

// In C++:
// Fallback timeout when running on slow bots.
constexpr base::TimeDelta kTimeoutOnSlowBots = base::Seconds(30);
```
