---
name: general/name-timer-callbacks-on-timer
rubric: readability
severity: should_fix
---

**Rule.** Do not name timer expiration callbacks or timer-triggered handlers
using initiation verbs like 'Schedule...'; name them with event-handler
conventions such as 'On...Timer' or 'OnTimer'. Reserve 'Schedule...' prefixes
for methods that arm a timer, post a task, or queue deferred work.

**Why.** Methods prefixed with 'Schedule...' imply that invoking them initiates,
queues, or schedules work to run in the future. When a method is itself the
callback executed upon timer expiration, a 'Schedule...' prefix misleads readers
about the control flow and call direction.

## Examples

Bad:

```cpp
// Bad: Using 'Schedule...' for a callback executed upon timer firing.
timer_.Start(FROM_HERE, kPollInterval, this,
             &MemoryMonitor::ScheduleMemoryCheck);

void MemoryMonitor::ScheduleMemoryCheck() {
  // Executes when timer fires.
  NotifyObservers(ComputeAvailableMemory());
}
```

Good:

```cpp
// Good: Using 'On...Timer' for the handler invoked when the timer expires.
timer_.Start(FROM_HERE, kPollInterval, this,
             &MemoryMonitor::OnMemoryCheckTimer);

void MemoryMonitor::OnMemoryCheckTimer() {
  // Executes when timer fires.
  NotifyObservers(ComputeAvailableMemory());
}
```
