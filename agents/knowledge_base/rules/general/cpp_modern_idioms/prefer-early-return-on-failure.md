---
name: general/prefer-early-return-on-failure
rubric: readability
severity: nit
---

**Rule.** Prefer early returns for failure guard clauses, error conditions, and
primary operation successes rather than wrapping execution paths in if-else
ladders or threading mutable state flags across fallback branches. When multiple
exit paths must record telemetry or logging before returning, encapsulate metric
emission in a dedicated helper function.

**Why.** Returning early keeps the happy path flat and unnested, separates
primary execution from fallback or recovery logic, and eliminates error-prone
mutable state tracking across branches. Encapsulating telemetry inside helper
functions ensures metrics are consistently recorded across distinct exit points
without duplicating histogram names.

## Examples

Bad:

```cpp
// Bad: wrapping success paths in else, or threading state flags across fallbacks
if (!is_valid) {
  return false;
} else {
  const bool is_replace_success = ::ReplaceFile(...);
  bool is_move_success = false;
  if (!is_replace_success) {
    is_move_success = ::MoveFile(...);
  }
  base::UmaHistogramBoolean("FileUtil.Success",
                            is_replace_success || is_move_success);
  return is_replace_success || is_move_success;
}
```

Good:

```cpp
// Good: early return on guard failure and primary operation success
if (!is_valid) {
  return false;
}

if (::ReplaceFile(...)) {
  RecordReplaceFileResult(/*replace_success=*/true,
                          /*move_success=*/std::nullopt);
  return true;
}

// Fallback path runs only when ReplaceFile fails.
const bool is_move_success = ::MoveFile(...);
RecordReplaceFileResult(/*replace_success=*/false, is_move_success);
return is_move_success;
```
