---
name: general/prefer-switch-over-chained-if-else
rubric: readability
severity: nit
---

**Rule.** When evaluating a single integral, enum, or error-code expression
against three or more discrete constant values, prefer a switch statement over
an if / else if / else chain.

**Why.** Using switch statements over chained equality checks avoids repeating
the tested variable or expression across multiple branches, clearly conveys
discrete multi-way value dispatch, and enables compiler jump-table
optimizations.

## Examples

Bad:

```cpp
if (error_code == ERROR_SUCCESS) {
  return ProcessSuccess();
} else if (error_code == ERROR_FILE_NOT_FOUND) {
  return ProcessFileNotFound();
} else if (error_code == ERROR_ACCESS_DENIED) {
  return ProcessAccessDenied();
} else {
  return ProcessGeneralError();
}
```

Good:

```cpp
switch (error_code) {
  case ERROR_SUCCESS:
    return ProcessSuccess();
  case ERROR_FILE_NOT_FOUND:
    return ProcessFileNotFound();
  case ERROR_ACCESS_DENIED:
    return ProcessAccessDenied();
  default:
    return ProcessGeneralError();
}
```
