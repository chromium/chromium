---
name: general/factor-shared-variant-over-suppressing-error-logging
rubric: design
severity: blocking
---

**Rule.** Do not modify an existing shared function to suppress error logging,
ignore specific failure codes, or conflate distinct error states when a new
caller considers a failure condition benign. Instead, preserve the original
function's error-logging contract and factor out a shared internal
implementation or non-logging variant (e.g., `FooIfExists()` or an explicit
error-handling parameter) for callers with benign failure expectations.

**Why.** Suppressing error logs or conflating error codes inside a shared
utility function degrades observability and diagnostic logging for all existing
callers that expect failures to be reported. Factoring out core logic into a
shared helper preserves accurate diagnostics while cleanly supporting callers
with differing error-handling requirements without code duplication.

## Examples

Bad:

```cpp
// Bad: Suppressing error logging in a shared function because one new caller expects ESRCH:
pid_t PIDForJob(const std::string& job_label) {
  int err = 0;
  pid_t pid = GetPIDInternal(job_label, &err);
  if (pid < 0 && err != ESRCH) {
    LOG(ERROR) << "PIDForJob: failed to get PID for " << job_label << ": " << err;
  }
  return pid;
}
```

Good:

```cpp
// Good: Factoring out a shared implementation and preserving error logging on the standard API:
pid_t PIDForJobIfLoaded(const std::string& job_label) {
  int err = 0;
  return GetPIDInternal(job_label, &err);
}

pid_t PIDForJob(const std::string& job_label) {
  pid_t pid = PIDForJobIfLoaded(job_label);
  if (pid < 0) {
    LOG(ERROR) << "PIDForJob: failed to get PID for " << job_label;
  }
  return pid;
}
```
