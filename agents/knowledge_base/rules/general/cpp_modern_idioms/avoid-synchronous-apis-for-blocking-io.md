---
name: general/avoid-synchronous-apis-for-blocking-io
rubric: readability
severity: should_fix
---

**Rule.** Do not expose public synchronous APIs for operations that perform
blocking file, disk, or hardware I/O; provide an asynchronous API (e.g., using
base::ThreadPool::PostTaskAndReply\*() or callbacks) instead. If synchronous
access is strictly necessary for performance on specific platforms where data is
in-memory and non-blocking, restrict the synchronous method using #if
BUILDFLAG(IS\_...) or base::PassKey, and document in the header comment that
synchronous retrieval is only supported on platforms where it can execute
without blocking I/O.

**Why.** Synchronous methods that perform blocking I/O (such as reading disk
files, sysfs, or hardware drivers) freeze the calling thread, causing UI jank
and responsiveness regressions on main or UI threads. Documenting the
non-blocking constraint on platform-restricted synchronous variants clarifies
the threading invariant and prevents contributors from adding blocking
synchronous implementations on other platforms.

## Examples

Bad:

```cpp
// Bad: Exposing a public synchronous API that performs blocking hardware I/O
// across all platforms, risking UI jank when called from the main thread.
class SysInfo {
 public:
  static std::string HardwareManufacturer() {
    return GetHardwareInfoSync().manufacturer;
  }
};
```

Good:

```cpp
// Good: Provide an asynchronous API via base::ThreadPool or restrict synchronous
// methods to non-blocking platforms while documenting the non-I/O requirement.
class SysInfo {
 public:
  static void GetHardwareInfo(base::OnceCallback<void(HardwareInfo)> callback);

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_IOS) || \
    BUILDFLAG(IS_CHROMEOS)
  // Returns the hardware manufacturer synchronously. This is only supported on
  // platforms where this can be done without blocking I/O operations.
  static std::string HardwareManufacturer();
#endif
};
```
