---
name: general/invert-return-condition-on-predicate-polarity-refactoring
rubric: correctness
severity: blocking
---

**Rule.** When defining, renaming, or refactoring a boolean predicate function
between positive polarity (e.g., ShouldInitialize..., ShouldEnable...,
IsValid...) and negative polarity (e.g., ShouldDefer..., ShouldSkip...,
IsInvalid...), verify that the return expression is inverted to match the new
semantics. Ensure that the returned condition does not contradict accompanying
explanatory doc comments or caller expectations.

**Why.** Failing to invert the return expression when refactoring a predicate's
polarity or name reverses the intended logical behavior, causing features or
initialization to run when they should be deferred or skipped (and vice versa).
Mismatches between function names, doc comments, and return conditions introduce
subtle correctness and startup bugs.

## Examples

Bad:

```cpp
// Deferring initialization until later saves startup time.
// Non-browser child processes should initialize immediately.
bool ChromeMainDelegate::ShouldInitializeFeature(std::string_view process_type) {
  // Bug: returns true for the browser process (empty process_type),
  // initializing early instead of deferring.
  return process_type.empty();
}
```

Good:

```cpp
// Deferring initialization until later saves startup time.
// Non-browser child processes should initialize immediately.
bool ChromeMainDelegate::ShouldInitializeFeature(std::string_view process_type) {
  // Correctly inverted: only initialize early for non-browser processes.
  return !process_type.empty();
}
```
