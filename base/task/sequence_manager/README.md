# What is this
This file documents high level parts of the sequence manager.

The sequence manager provides a set of prioritized FIFO task queues, which
allows funneling multiple sequences of immediate and delayed tasks on a single
underlying sequence.

## Work Queue and Task selection
Both immediate tasks and delayed tasks are posted to a `TaskQueue` via an
associated `TaskRunner`. `TaskQueue`s use distinct primitive FIFO queues, called
`WorkQueue`s, to manage immediate tasks and delayed tasks. Tasks eventually end
up in their assigned `WorkQueue` which is made directly visible to
`SequenceManager` through `TaskQueueSelector`.
`SequenceManagerImpl::SelectNextTask()` uses
`TaskQueueSelector::SelectWorkQueueToService()` to select the next work queue
based on various policy e.g. priority, from which 1 task is popped at a time.

## Journey of a Task
Task queues have a mechanism to allow efficient cross-thread posting with the
use of 2 work queues, `immediate_incoming_queue` which is used when posting, and
`immediate_work_queue` used to pop tasks from. An immediate task posted from the
main thread is pushed on `immediate_incoming_queue` in
`TaskQueueImpl::PostImmediateTaskImpl()`. If the work queue was empty,
`SequenceManager` is notified and the `TaskQueue` is registered to do
`ReloadEmptyImmediateWorkQueue()` before SequenceManager selects a task, which
moves tasks from `immediate_incoming_queue` to `immediate_work_queue` in batch
for all registered `TaskQueue`s. The tasks then follow the regular work queue
selection mechanism.

## Journey of a WakeUp
A `WakeUp` represents a time at which a delayed task wants to run.

Each `TaskQueueImpl` maintains its own next wake-up as
`main_thread_only().scheduled_wake_up`, associated with the earliest pending
delayed task. It communicates its wake up to the WakeUpQueue via
`WakeUpQueue::SetNextWakeUpForQueue()`. The `WakeUpQueue` is responsible for
determining the single next wake up time for the thread. This is accessed from
`SequenceManagerImpl` and may determine the next run time if there's no
immediate work, which ultimately gets passed to the MessagePump, typically via
`MessagePump::Delegate::NextWorkInfo` (returned by
`ThreadControllerWithMessagePumpImpl::DoWork()`) or by
`MessagePump::ScheduleDelayedWork()` (on rare occasions where the next WakeUp is
scheduled on the main thread from outside a `DoWork()`). When a delayed run time
associated with a wake-up is reached, `WakeUpQueue` is notified through
`WakeUpQueue::MoveReadyDelayedTasksToWorkQueues()` and in turn notifies all
`TaskQueue`s whose wake-up can be resolved. This lets each `TaskQueue`s process
ripe delayed tasks.

## Journey of a delayed Task
A delayed Task posted cross-thread generates an immediate Task to run
`TaskQueueImpl::ScheduleDelayedWorkTask()` which eventually calls
`TaskQueueImpl::PushOntoDelayedIncomingQueueFromMainThread()`, so that it can be
enqueued on the main thread. A delayed Task posted from the main thread skips
this step and calls
`TaskQueueImpl::PushOntoDelayedIncomingQueueFromMainThread()` directly. The Task
is then pushed on `main_thread_only().delayed_incoming_queue` and possibly
updates the next task queue wake-up. Once the delayed run time is reached,
possibly because the wake-up is resolved, the delayed task is moved to
`main_thread_only().delayed_work_queue` and follows the regular work queue
selection mechanism.

## TimeDomain and TickClock
`SequenceManager` and related classes use a common `TickClock` that can be
injected by specifying a `TimeDomain`. A `TimeDomain` is a specialisation of
`TickClock` that gets notified when the `MessagePump` is about to go idle via
TimeDomain::MaybeFastForwardToWakeUp(), and can use the signal to fast forward
in time. This is used in `TaskEnvironment` to support `MOCK_TIME`, and in
devtools to support virtual time.
