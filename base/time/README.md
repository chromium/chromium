# //base: Time-Related Functionality

This directory contains the portions of //base that deal with time-related
concepts. Most critical are the classes in [time.h](time.h).
 - `Time` represents a specific wall-clock time. It is computed from the system
clock, meaning successive requests for the current time might not always
monotonically increase (e.g. across automatic or manual clock adjustments).
Generally it is appropriate for anything human-visible, e.g. the last modified
date/time of a file or a future time when Chrome will be automatically
restarted, but users must safely handle negative durations and other effects of
the non-monotonic clock.
 - `TimeTicks` is computed from an incrementing counter. It thus increases
monotonically, meaning it's usually appropriate for determining how much time
elapses between two nearby events, e.g. for function timing for profiling, or to
schedule a task "100 milliseconds from now", regardless of what the clock reads
at that point. However, its behavior across power-saving mode changes is
platform-dependent, meaning it may not increment during times when the system
clock continues to run, and the precise conditions under which it does increment
vary by platform. This usually makes it inappropriate for long durations,
especially in cross-platform code; for example, a histogram that uses
`TimeTicks` to count events in a thirty-day window will show very different
results on a platform that pauses the counter during sleep compared to one where
it continues to run. It is also non-sensical to try and convert a `TimeTicks` to
a `Time` and then use that as a reference point for any other `TimeTicks` value,
since even within the same process, both intervening sleeps and intervening
clock adjustments may mean the values should have had different reference points.
 - `TimeDelta` represents a duration between two Times or TimeTicks.

There are also various files dealing with clocks, which are primarily useful
when tests need to modify how the program tracks the passage of time. See
[/base/test/task_environment.h](/base/test/task_environment.h)'s `MOCK_TIME`
ability for
[testing components which post tasks](/docs/threading_and_tasks_testing.md).
