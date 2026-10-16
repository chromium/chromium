---
name: general/prefer-reset-over-nullopt-assignment-for-optional
rubric: readability
severity: nit
---

**Rule.** Prefer calling `.reset()` on an `std::optional` instance to clear or
disengage its value rather than assigning `= std::nullopt` or `= {}`.

**Why.** Calling `.reset()` directly conveys the intent to disengage and destroy
the optional's contained value, produces cleaner and more concise code, and
avoids redundantly referencing the `std::nullopt` tag type.

## Examples

Bad:

```cpp
void MessagePumpUIApplication::ResetNextInitialNestingLevelForTesting() {
  g_initial_nesting_level = std::nullopt;
}
```

Good:

```cpp
void MessagePumpUIApplication::ResetNextInitialNestingLevelForTesting() {
  g_initial_nesting_level.reset();
}
```
