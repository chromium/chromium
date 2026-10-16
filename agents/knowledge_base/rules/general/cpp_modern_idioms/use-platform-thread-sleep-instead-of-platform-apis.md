---
name: general/use-platform-thread-sleep-instead-of-platform-apis
rubric: readability
severity: should_fix
---

**Rule.** Do not call platform-specific sleep functions such as Win32 ::Sleep()
or POSIX sleep(), usleep(), or nanosleep(). Use base::PlatformThread::Sleep()
with a strongly typed base::TimeDelta (e.g., base::Milliseconds(...)) instead.

**Why.** Platform-specific sleep functions take raw integer units with differing
time resolutions across operating systems and bypass Chromium's threading
abstractions. base::PlatformThread::Sleep() accepts strongly typed
base::TimeDelta durations and handles cross-platform differences such as looping
across premature wakeups under high-resolution Windows clocks.

## Examples

Bad:

```cpp
constexpr int kDelayMs = 50;
::Sleep(kDelayMs);
```

Good:

```cpp
constexpr int kDelayMs = 50;
base::PlatformThread::Sleep(base::Milliseconds(kDelayMs));
```
