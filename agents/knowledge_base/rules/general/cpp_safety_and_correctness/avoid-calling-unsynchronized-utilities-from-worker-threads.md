---
name: general/avoid-calling-unsynchronized-utilities-from-worker-threads
rubric: safety
severity: should_fix
---

**Rule.** Do not call utility functions or wrappers that read or mutate
unsynchronized process-global state (such as static test override pointers) from
background threads or ThreadPool workers, even if the underlying platform API is
thread-safe. Callers on worker threads must query the underlying thread-safe
platform API directly or pass required values via parameters.

**Why.** Utility wrappers (such as base::apple::MainBundleIdentifier()) often
maintain unsynchronized static pointers (e.g. g_override_main_bundle) for test
mocking. Calling these helpers from concurrent worker threads creates data races
with test setup and concurrent operations, whereas the underlying operating
system APIs (like NSBundle.mainBundle) are thread-safe.

## Examples

Bad:

```cpp
// On a background worker thread or ThreadPool task:
base::ThreadPool::PostTask(
    FROM_HERE, {base::TaskPriority::BEST_EFFORT},
    base::BindOnce([]() {
      // BAD: MainBundleIdentifier reads unsynchronized g_override_main_bundle.
      std::string id = base::apple::MainBundleIdentifier();
    }));
```

Good:

```cpp
// On a background worker thread or ThreadPool task:
base::ThreadPool::PostTask(
    FROM_HERE, {base::TaskPriority::BEST_EFFORT},
    base::BindOnce([]() {
      // GOOD: Bypass unsynchronized wrapper and query thread-safe OS API directly.
      std::string id =
          base::SysNSStringToUTF8(NSBundle.mainBundle.bundleIdentifier);
    }));
```
