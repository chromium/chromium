---
name: general/update-callers-and-contracts-on-sentinel-changes
rubric: correctness
severity: should_fix
---

**Rule.** When modifying the return values, error codes, or sentinels of a
helper or callee function, audit and update all callers and wrapper functions.
Ensure callers check for the updated sentinel values and translate or map
internal error codes appropriately to uphold their own documented API return
contracts rather than leaking internal sentinels.

**Why.** Failing to update callers after changing a helper's sentinel or error
code leads to bypassed error handling, misdiagnosed failures, and internal
sentinel leakage that violates the caller's documented API return contract and
breaks downstream consumers.

## Examples

Bad:

```cpp
pid_t PIDForJob(const std::string& job_label) {
  pid_t pid = PIDForJobIfLoaded(job_label);
  // Bad: PIDForJobIfLoaded was changed to return -2 when unloaded, but caller still checks -1
  if (pid == -1) {
    LOG(ERROR) << "PIDForJob: job not loaded";
  }
  return pid;  // Leaks -2 to callers expecting -1 on error
}
```

Good:

```cpp
pid_t PIDForJob(const std::string& job_label) {
  pid_t pid = PIDForJobIfLoaded(job_label);
  // Good: Check for the updated internal sentinel and map to the caller's documented contract
  if (pid == -2) {
    LOG(ERROR) << "PIDForJob: error " << ESRCH;
    return -1;  // Conforms to PIDForJob's contract of returning -1 on error
  }
  return pid;
}
```
