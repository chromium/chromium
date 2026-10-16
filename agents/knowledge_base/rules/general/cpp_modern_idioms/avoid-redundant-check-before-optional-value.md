---
name: general/avoid-redundant-check-before-optional-value
rubric: readability
severity: should_fix
---

**Rule.** Do not write redundant `CHECK(opt.has_value())` or
`DCHECK(opt.has_value())` assertions immediately before accessing an
`std::optional` or `base::expected` with `.value()`. Calling `.value()` already
performs a checked access that aborts if the object is empty; call `.value()`
directly, or use `*opt` / `opt->` when dereferencing after an earlier guard.

**Why.** `std::optional::value()` and `base::expected::value()` already verify
that a value is present and abort execution if the container is empty. Preceding
`.value()` with a manual `CHECK(has_value())` adds visual clutter, duplicate
branching, and redundant assertions.

## Examples

Bad:

```cpp
CHECK(replace_error_code.has_value());
switch (replace_error_code.value()) {
  case ERROR_FILE_NOT_FOUND: {
    CHECK(is_move_success.has_value());
    result = is_move_success.value()
                 ? ReplaceFileResult::kFileNotFoundMoveSuccess
                 : ReplaceFileResult::kFileNotFoundMoveFailed;
    break;
  }
}
```

Good:

```cpp
switch (replace_error_code.value()) {
  case ERROR_FILE_NOT_FOUND:
    result = is_move_success.value()
                 ? ReplaceFileResult::kFileNotFoundMoveSuccess
                 : ReplaceFileResult::kFileNotFoundMoveFailed;
    break;
}
```
