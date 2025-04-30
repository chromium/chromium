# HangWatcher

HangWatcher is a mechanism for detecting hangs in Chrome, logging their
frequency and nature in UMA and uploading crash reports.

## Definition of a hang
In this document a hang is qualified as any scope that does not complete
within a certain wall-time allowance. A scope is defined by the lifetime
of a `WatchHangsInScope` object. The time-out value can be different for
each individual scope.

### Example 1
A task on the IO thread encounters a lock on which it blocks for 20s.
There is absolutely no progress made as the OS is bound to deschedule
the thread while the contention on the lock remains. This is a hang.

### Example 2
A small function that should execute relatively quickly spends 30s
burning CPU without making any outwardly visible progress. In this
case there is progress made by the thread in a sense, since the
[program counter](https://en.wikipedia.org/wiki/Program_counter)
is not static for the duration of the time-out. However, as far as
Chrome, and critically its user, is concerned we are stuck and not
making progress. This is a hang.

### Example 3
A message pump is busy pumping millions of tasks and dispatches
them quickly. The task at the end of the queue has to wait for up
to 30s to get executed. This is not a hang. This is congestion.
See //content/scheduler/responsiveness for more details.

## Design

Hangs are monitored by one thread per process. This is a thread in
the OS sense. It is not based on `base::Thread` and does not use
the task posting APIs.

Other threads that want to be monitored register with this watcher
thread. This can be done at thread creation or at any other time.

Monitored threads do not have any responsibilities apart from
marking the entering and leaving of monitored scopes. This is
done using a `WatchHangsInScope` object that is instantiated
on the stack, at the beginning of the scope.

### Example:

```
void FooBar(){
  WatchHangsInScope scope(base::TimeDelta::FromSeconds(5));
  DoWork();
}
```


The HangWatcher thread periodically traverses the list of
registered threads and verifies that they are not hung
within a monitored scope.

```
+-------------+       +-----------------+                       +-----------------+
| HangWatcher |       | WatchedThread1  |                       | WatchedThread2  |
+-------------+       +-----------------+                       +-----------------+
       |                       |                                         |
       | Init()                |                                         |
       |-------                |                                         |
       |      |                |                                         |
       |<------                |                                         |
       |                       |                                         |
       |            Register() |                                         |
       |<----------------------|                                         |
       |                       |                                         |
       |                       |                              Register() |
       |<----------------------------------------------------------------|
       |                       |                                         |
       |                       |                                         | SetDeadline()
       |                       |                                         |--------------
       |                       |                                         |             |
       |                       |                                         |<-------------
       |                       |                                         |
       |                       |                                         | ClearDeadline()
       |                       |                                         |----------------
       |                       |                                         |               |
       |                       |                                         |<---------------
       |                       |                                         |
       | Monitor()             |                                         |
       |---------------------->|                                         |
       |                       | ------------------------\               |
       |                       |-| No deadline, no hang. |               |
       |                       | |-----------------------|               |
       |                       |                                         |
       | Monitor()             |                                         |
       |---------------------------------------------------------------->|
       |                       |                                         | ------------------------\
       |                       |                                         |-| No deadline, no hang. |
       |                       |                                         | |-----------------------|
       |                       |                                         |
       |                       | SetDeadline()                           |
       |                       |--------------                           |
       |                       |             |                           |
       |                       |<-------------                           |
       |                       |                                         |
       | Monitor()             |                                         |
       |---------------------->| -------------------------------\        |
       |                       |-| Live expired deadline. Hang! |        |
       |                       | |------------------------------|        |
       |                       |                                         |
       | RecordHang()          |                                         |
       |-------------          |                                         |
       |            |          |                                         |
       |<------------          |                                         |
       |                       |                                         |
```

## Protections against non-actionable reports

### Ignoring normal long running code

There are cases where code is expected to take a long time to complete.
It's possible to keep such cases from triggering the detection of a hang.
Invoking `HangWatcher::InvalidateActiveExpectations()` from within a
scope will make sure that not hangs are logged while execution is within it.

### Example:

```
void RunTask(Task task) {
  // In general, tasks shouldn't hang.
  WatchHangsInScope scope(base::TimeDelta::FromSeconds(5));

  std::move(task.task).Run();  // Calls `TaskKnownToBeVeryLong`.
}

void TaskKnownToBeVeryLong() {
  // This particular function is known to take a long time. Never report it as a
  // hang.
  HangWatcher::InvalidateActiveExpectations();

  BlockWaitingForUserInput();
}
```

### Protections against wrongfully blaming code

TODO

### Ignoring system suspend

TODO
