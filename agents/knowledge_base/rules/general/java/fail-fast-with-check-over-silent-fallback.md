---
name: general/fail-fast-with-check-over-silent-fallback
rubric: readability
severity: should_fix
---

**Rule.** Do not wrap code in defensive runtime if-checks, loose inequality
checks (such as \<= MIN), silent fallback branches, silent early returns, or
speculative 'just in case' workarounds when internal preconditions, parameter
constraints, container states, pointers, numeric bounds, or bootstrapped
dependencies are invariant to always hold. Enforce invariants and parameter
constraints using CHECK() / DCHECK() in C++ (e.g., CHECK(pointer),
DCHECK(items.empty()), CHECK_GE(depth, kMin), DCHECK_EQ(depth, 0)) or assert /
assumeNonNull in Java (e.g., assert state != UNBOUND,
assumeNonNull(binding).isBound()) rather than silently tolerating unexpected
states or returning fallbacks.

**Why.** Defensive runtime if-checks, loose inequalities, silent fallback
branches, and speculative workarounds obscure code invariants and mask caller
bugs, re-entrancy issues, or corrupted state during development and testing.
Asserting preconditions and invariants with CHECK(), DCHECK(), Java assert, or
assumeNonNull documents programmer expectations, keeps control flow direct, and
surfaces contract violations deterministically in tests and crash reports.

## Examples

Bad:

```cpp
// Bad: Defensive if-check or silent fallback when invariant must hold.
if (g_provider) {
  return g_provider->ToString();
}
return StackTrace().ToString();

if (depth <= kDefaultInitialNestingLevel) {
  nesting_level_ = 1;
  return;
}

// Bad: Tolerating hypothetical edge case instead of asserting invariant.
// Do not reset `observer_depth` just in case there is re-entrancy during teardown.
g_spin_detector.consecutive_hollow_calls = 0;

// Bad: Defensive null check or silent early return on invariant violation in Java.
if (mNotPerceptibleBinding != null && !mNotPerceptibleBinding.isBound()) {
  mNotPerceptibleBinding.bindServiceConnection();
}
```

Good:

```cpp
// Good: Assert invariant directly with (D)CHECK and execute unconditionally.
CHECK(g_provider);
return g_provider->GetStackTraceString();

// Good: Fail fast on lower-bound invariant violations with relational assertions.
CHECK_GE(depth, kDefaultInitialNestingLevel);
if (depth == kDefaultInitialNestingLevel) {
  nesting_level_ = 1;
  return;
}

// Good: Explicitly assert expected invariant state rather than tolerating unknowns.
DCHECK_EQ(g_spin_detector.observer_depth, 0);
g_spin_detector.consecutive_hollow_calls = 0;

// Good: Assert non-null invariant with assumeNonNull and preconditions with assert.
if (!assumeNonNull(mNotPerceptibleBinding).isBound()) {
  assumeNonNull(mNotPerceptibleBinding).bindServiceConnection();
}
```
