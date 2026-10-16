---
name: general/avoid-arbitrary-sleeps-in-multithreaded-unit-tests
rubric: readability
severity: should_fix
---

**Rule.** In multithreaded unit tests and concurrency stress tests, avoid using
base::PlatformThread::Sleep() or arbitrary wall-clock delays to govern test
execution or wait for worker threads. Use a fixed, deterministic number of
iterations or explicit synchronization primitives (such as base::WaitableEvent,
base::RunLoop, or completion callbacks) to coordinate thread lifetimes and
termination.

**Why.** Arbitrary wall-clock sleeps unnecessarily increase test suite runtimes
and introduce test flakiness on heavily loaded trybot runners, where worker
threads may not be scheduled frequently enough before the sleep expires.
Deterministic loop counts and explicit synchronization ensure fast, reproducible
test execution.

## Examples

Bad:

```cpp
// Flaky and slow: relies on arbitrary wall-clock sleep to let threads run.
for (int i = 0; i < 20; ++i) {
  threads.push_back(std::make_unique<WorkerThread>(&event, i % 5));
  threads.back()->Start();
}
event.Signal();
base::PlatformThread::Sleep(base::Milliseconds(500));
stop = true;
for (auto& thread : threads) {
  thread->Join();
}
```

Good:

```cpp
// Deterministic: workers execute a fixed number of iterations.
for (int i = 0; i < 20; ++i) {
  threads.push_back(
      std::make_unique<WorkerThread>(&event, i % 5, /*iterations=*/1000));
  threads.back()->Start();
}
event.Signal();
for (auto& thread : threads) {
  thread->Join();
}
```
