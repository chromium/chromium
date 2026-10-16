---
name: general/invoke-plog-or-save-errno-immediately
rubric: correctness
severity: should_fix
---

**Rule.** When logging failed system calls or platform APIs with PLOG, DPLOG,
VPLOG, or inspecting system error state (errno or ::GetLastError()), invoke the
logging macro or capture the error code immediately after the failing call. Do
not execute intervening operations—such as histogram recording
(base::UmaHistogram\*), memory allocations, locks, or cleanup routines—prior to
reading the error, as they can clobber errno or GetLastError() and produce
misleading error logs.

**Why.** PLOG and related macros inspect thread-local error state (errno on
POSIX and ::GetLastError() on Windows). Intermediate function calls—including
metric logging, memory allocation, or standard library routines—can invoke
system calls or runtime helpers that overwrite errno, causing PLOG to log an
unrelated error code or report Success.

## Examples

Bad:

```cpp
bool supported = madvise(nullptr, 0, madvise_advice) == 0;
// Intervening histogram recording can clobber errno before PLOG runs.
base::UmaHistogramBoolean("Android.Prefetch.Madvise", supported);
if (!supported) {
  PLOG(WARNING) << "madvise not supported";
}
```

Good:

```cpp
bool supported = madvise(nullptr, 0, madvise_advice) == 0;
if (!supported) {
  // Log immediately before intervening calls can overwrite errno.
  PLOG(WARNING) << "madvise not supported";
}
base::UmaHistogramBoolean("Android.Prefetch.Madvise", supported);
```
