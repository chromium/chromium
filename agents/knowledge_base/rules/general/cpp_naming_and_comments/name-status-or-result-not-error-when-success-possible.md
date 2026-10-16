---
name: general/name-status-or-result-not-error-when-success-possible
rubric: readability
severity: optional
---

**Rule.** Do not name a variable, parameter, or field `error` or `error_code` if
it can represent or carry a success state (such as `ERROR_SUCCESS`, `net::OK`,
or `0`). Name the identifier `result`, `status`, or a domain-specific outcome
name instead.

**Why.** Naming a status code variable `error` or `error_code` when it can
represent success is misleading at call sites and in condition checks (e.g.,
passing `/*error_code=*/ERROR_SUCCESS` or checking
`if (error_code == ERROR_SUCCESS)`). Using `result` or `status` accurately
reflects the entire domain of possible outcomes.

## Examples

Bad:

```cpp
void RecordReplaceFileResult(bool is_backup_path_valid,
                             std::optional<DWORD> replace_error_code,
                             std::optional<bool> is_move_success);

// Call site passing success code into error_code parameter:
RecordReplaceFileResult(/*is_backup_path_valid=*/true,
                        /*replace_error_code=*/ERROR_SUCCESS,
                        /*is_move_success=*/std::nullopt);
```

Good:

```cpp
void RecordReplaceFileResult(bool is_backup_path_valid,
                             std::optional<DWORD> replace_result,
                             std::optional<bool> is_move_success);

// Call site passing success code into result parameter:
RecordReplaceFileResult(/*is_backup_path_valid=*/true,
                        /*replace_result=*/ERROR_SUCCESS,
                        /*is_move_success=*/std::nullopt);
```
