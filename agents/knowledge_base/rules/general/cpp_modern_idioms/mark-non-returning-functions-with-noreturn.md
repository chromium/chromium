---
name: general/mark-non-returning-functions-with-noreturn
rubric: readability
severity: should_fix
---

**Rule.** When declaring or defining functions, destructors, or helper routines
that unconditionally terminate execution (such as crash helpers, fatal check
failure handlers, or functions that unconditionally invoke `NOTREACHED()` or
`base::ImmediateCrash()`), annotate the declaration with the standard
`[[noreturn]]` attribute. Do not rely on compiler warning suppressions or dummy
return statements in non-returning functions.

**Why.** `[[noreturn]]` explicitly communicates to the compiler and static
analysis tools that control flow never leaves the function. This prevents
false-positive diagnostics (such as missing return statements or unreached code
warnings in callers) and enables the optimizer to eliminate unreachable branches
and optimize call sites accordingly.

## Examples

Bad:

```cpp
// Bad: Terminating crash helper declared without [[noreturn]].
void HandleFatalCheckFailure(const char* message) {
  LOG(FATAL) << message;
}

int GetValidatedValue(int x) {
  if (x < 0) {
    HandleFatalCheckFailure("Negative value");
    // Compiler may warn about missing return statement here.
  }
  return x * 2;
}
```

Good:

```cpp
// Good: Terminating crash helper annotated with [[noreturn]].
[[noreturn]] void HandleFatalCheckFailure(const char* message) {
  LOG(FATAL) << message;
}

int GetValidatedValue(int x) {
  if (x < 0) {
    HandleFatalCheckFailure("Negative value");
  }
  return x * 2;
}
```
