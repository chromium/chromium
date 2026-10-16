---
name: general/recheck-error-condition-in-retry-loop
rubric: correctness
severity: should_fix
---

**Rule.** When retrying an operation conditioned on a specific recoverable or
transient error (such as transient resource exhaustion or temporary system
errors), re-check the error code or condition after every failed attempt inside
the retry loop. Terminate the loop early (e.g. via break or returning an error)
if a subsequent attempt fails with an unexpected or unrecoverable error code.

**Why.** If an operation in a retry loop fails due to a different,
non-recoverable error (such as invalid arguments or permissions), subsequent
retries cannot succeed. Continuing to retry wastes CPU time, causes unnecessary
latency, and may execute destructive recovery side effects (such as terminating
background processes or freeing caches).

## Examples

Bad:

```cpp
// Retrying file mapping creation on system commit limit.
for (int tries = 0; tries < kMaxTries; ++tries) {
  partition_alloc::TerminateAnotherProcessOnCommitFailure();
  base::PlatformThread::Sleep(kDelay);

  handle = CreateFileMapping(INVALID_HANDLE_VALUE, ...);
  if (handle) {
    break;
  }
}
```

Good:

```cpp
// Retrying file mapping creation on system commit limit.
for (int tries = 0; tries < kMaxTries; ++tries) {
  partition_alloc::TerminateAnotherProcessOnCommitFailure();
  base::PlatformThread::Sleep(kDelay);

  handle = CreateFileMapping(INVALID_HANDLE_VALUE, ...);
  if (handle) {
    break;
  }

  // Bail out early if failure is due to an unrecoverable error.
  if (::GetLastError() != ERROR_COMMITMENT_LIMIT) {
    break;
  }
}
```
