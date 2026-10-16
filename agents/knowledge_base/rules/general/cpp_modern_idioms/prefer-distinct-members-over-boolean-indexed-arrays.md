---
name: general/prefer-distinct-members-over-boolean-indexed-arrays
rubric: readability
severity: nit
---

**Rule.** Declare distinct, self-describing member variables (e.g.,
num_foreground_task_sources\_, num_background_task_sources\_) rather than small
fixed-size arrays indexed by boolean expressions, boolean literals ([false],
[true]), or boolean-to-integer conversions. If collection iteration or dynamic
indexing is genuinely required, use strongly typed enums with dedicated accessor
helpers or explicit named constants.

**Why.** Indexing arrays with booleans or conditional conversions creates
cryptic call sites (such as arr[false] or arr[cond]), obscures the semantic
meaning of each slot, and makes state inspection difficult. Distinct named
member variables make code self-documenting and eliminate fragile indexing
logic.

## Examples

Bad:

```cpp
// Bad: Small array indexed by boolean literals or conditional expressions.
std::array<size_t, 2> num_task_sources_ = {};

size_t GetNumForegroundTaskSources() const {
  return num_task_sources_[false];
}

void Increment(ThreadType type) {
  ++num_task_sources_[type == ThreadType::kBackground];
}
```

Good:

```cpp
// Good: Distinct, self-describing member variables.
size_t num_foreground_task_sources_ = 0;
size_t num_background_task_sources_ = 0;

size_t GetNumForegroundTaskSources() const {
  return num_foreground_task_sources_;
}

void Increment(ThreadType type) {
  if (type == ThreadType::kBackground) {
    ++num_background_task_sources_;
  } else {
    ++num_foreground_task_sources_;
  }
}
```
