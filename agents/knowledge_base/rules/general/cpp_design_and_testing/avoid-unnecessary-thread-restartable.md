---
name: general/avoid-unnecessary-thread-restartable
rubric: design
severity: should_fix
---

**Rule.** Do not pass `Thread::Restartable{}` when constructing or subclassing
`base::Thread` unless the thread is explicitly required to be stopped and
re-started during its lifecycle. By default, instantiate `base::Thread` without
`Restartable` to enforce single-lifecycle execution.

**Why.** base::Thread instances are non-restartable by default to ensure safe
lifecycle management and prevent issues like rebinding delegates. Passing
Thread::Restartable{} when a thread is never restarted introduces unnecessary
configurability and misleads readers about the thread's lifecycle.

## Examples

Bad:

```cpp
// Bad: Opting into restartability for a thread that is never restarted.
ServiceThread::ServiceThread()
    : Thread("ThreadPoolServiceThread", Thread::Restartable{}) {}
```

Good:

```cpp
// Good: Default to non-restartable Thread for single-lifecycle threads.
ServiceThread::ServiceThread() : Thread("ThreadPoolServiceThread") {}
```
