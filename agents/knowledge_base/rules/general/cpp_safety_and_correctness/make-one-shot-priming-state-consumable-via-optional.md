---
name: general/make-one-shot-priming-state-consumable-via-optional
rubric: correctness
severity: should_fix
---

**Rule.** When using an out-of-band setter to prime or configure a single
upcoming operation or instance (such as static or thread-local state when direct
parameter passing is unavailable), store the state in an std::optional<T>,
assert that the state is unset (e.g. with CHECK(!state.has_value())) before
writing in the SetNext... setter, and consume it immediately on first access
using std::exchange(state, std::nullopt).

**Why.** Persistent or sentinel-defaulted global and thread-local state can leak
across operations if early returns or exceptional control flow bypass manual
reset logic. Storing single-use configuration in an std::optional that is
cleared on read guarantees strictly one-shot execution, asserting unset state in
the setter prevents conflicting callers from silently overwriting unconsumed
settings, and SetNext... naming communicates the consumable contract to callers.

## Examples

Bad:

```cpp
// Persistent thread-local state with manual reset at the end of execution.
thread_local int g_initial_nesting_level = kDefaultInitialNestingLevel;

void MessagePumpUIApplication::SetInitialNestingLevel(int depth) {
  // Missing assertion that previous priming state was consumed.
  g_initial_nesting_level = depth;
}

void MessagePumpCFRunLoopBase::OnAttach() {
  if (g_initial_nesting_level <= kDefaultInitialNestingLevel) {
    return;  // Early exit leaves state unreset for subsequent operations.
  }
  // ...
  g_initial_nesting_level = kDefaultInitialNestingLevel;
}
```

Good:

```cpp
// Consumable thread-local state with explicit one-shot setter naming and assertions.
thread_local std::optional<int> g_initial_nesting_level;

void MessagePumpUIApplication::SetNextInitialNestingLevelForCurrentThread(
    int depth) {
  CHECK(!g_initial_nesting_level.has_value());
  CHECK_GE(depth, kDefaultInitialNestingLevel);
  g_initial_nesting_level = depth;
}

void MessagePumpCFRunLoopBase::OnAttach() {
  // Consumed immediately on read; subsequent operations start clean.
  std::optional<int> initial_depth =
      std::exchange(g_initial_nesting_level, std::nullopt);
  int depth = initial_depth.value_or(kDefaultInitialNestingLevel);
  // ...
}
```
