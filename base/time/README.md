# //base: Time-Related Functionality

This directory contains the portions of //base that deal with time-related
concepts. Most critical are the classes in [time.h](time.h). `Time` and
`TimeTicks` both encode absolute times, but `TimeTicks` is monotonic and should
be used for most internal purposes, while `Time` can move backwards and is
primarily for human-readable times. `TimeDelta` is a duration computed from
either of the above concepts.

There are also various files dealing with clocks, which are primarily useful
when tests need to modify how the program tracks the passage of time. See
[/base/test/task_environment.h](/base/test/task_environment.h)'s `MOCK_TIME`
ability for
[testing components which post tasks](/docs/threading_and_tasks_testing.md).
