---
name: general/assert-mutual-exclusivity-of-execution-modes
rubric: correctness
severity: should_fix
---

**Rule.** When defining, computing, or branching on multiple boolean flags,
execution modes, configuration options, or paired annotations (such as
`@Enable...` and `@Disable...`) intended to be mutually exclusive, explicitly
assert or validate their mutual exclusivity (e.g., using
`CHECK(!(mode_a && mode_b))` or `DCHECK` in C++, or throwing an
`IllegalStateException` or asserting in Java). Do not rely solely on sequential
`if ... else if` chaining or silent priority overrides without verifying that
contradictory options cannot simultaneously be active.

**Why.** Subtle edge cases, invalid configurations, or conflicting annotations
can inadvertently activate contradictory options or execution modes
simultaneously. Explicitly validating mutual exclusivity prevents ambiguity,
halts execution before invalid compound states produce undefined behavior, and
immediately surfaces conflicting developer intent instead of silently masking
errors.

## Examples

Bad:

```cpp
// C++: sequential if-else silently assumes mutual exclusivity
if (synchronous_preread_required) {
  PreReadMainDllSynchronously();
} else if (asynchronous_preread_required) {
  PreReadMainDllAsynchronously();
}

// Java: silently letting one annotation take precedence over another
boolean enabled = testClass.getAnnotation(EnableLeakChecks.class) != null;
boolean disabled = testClass.getAnnotation(DisableLeakCheck.class) != null;
if (enabled || enabledByFlag) {
  if (disabled) return;
  checkLeaks();
}
```

Good:

```cpp
// C++: assert mutual exclusivity between alternative execution modes
const bool sync_preread = ShouldPreReadSynchronously();
const bool async_preread = ShouldPreReadAsynchronously();
CHECK(!(sync_preread && async_preread));
if (sync_preread) {
  PreReadMainDllSynchronously();
} else if (async_preread) {
  PreReadMainDllAsynchronously();
}

// Java: validate mutually exclusive paired annotations and fail fast
boolean enabled = testClass.getAnnotation(EnableLeakChecks.class) != null;
boolean disabled = testClass.getAnnotation(DisableLeakCheck.class) != null;
if (enabled && disabled) {
  throw new IllegalStateException("Both @EnableLeakChecks and @DisableLeakCheck specified on " + testClass.getName());
}
```
