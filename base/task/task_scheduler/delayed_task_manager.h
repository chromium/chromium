// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_TASK_SCHEDULER_DELAYED_TASK_MANAGER_H_
#define BASE_TASK_TASK_SCHEDULER_DELAYED_TASK_MANAGER_H_

#include <memory>
#include <queue>
#include <utility>
#include <vector>

#include "base/base_export.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/atomic_flag.h"
#include "base/task/task_scheduler/scheduler_lock.h"
#include "base/task/task_scheduler/task.h"
#include "base/time/default_tick_clock.h"
#include "base/time/tick_clock.h"

namespace base {

class TaskRunner;

namespace internal {

// The DelayedTaskManager forwards tasks to post task callbacks when they become
// ripe for execution. Tasks are not forwarded before Start() is called. This
// class is thread-safe.
class BASE_EXPORT DelayedTaskManager {
 public:
  // Posts |task| for execution immediately.
  using PostTaskNowCallback = OnceCallback<void(Task task)>;

  // |tick_clock| can be specified for testing.
  DelayedTaskManager(std::unique_ptr<const TickClock> tick_clock =
                         std::make_unique<DefaultTickClock>());
  ~DelayedTaskManager();

  // Starts the delayed task manager, allowing past and future tasks to be
  // forwarded to their callbacks as they become ripe for execution.
  // |service_thread_task_runner| posts tasks to the TaskScheduler service
  // thread.
  void Start(scoped_refptr<TaskRunner> service_thread_task_runner);

  // Schedules a call to |post_task_now_callback| with |task| as argument when
  // |task| is ripe for execution.
  void AddDelayedTask(Task task, PostTaskNowCallback post_task_now_callback);

 private:
  struct DelayedTask {
    DelayedTask(Task task, PostTaskNowCallback callback);
    DelayedTask(DelayedTask&& other);
    ~DelayedTask();

    // Required by std::priority_queue::pop().
    DelayedTask& operator=(DelayedTask&& other);

    // Required by std::priority_queue.
    bool operator>(const DelayedTask& other) const;

    Task task;
    PostTaskNowCallback callback;

    // True iff the delayed task has been marked as scheduled.
    bool IsScheduled() const;

    // Mark the delayed task as scheduled. Since the sort key is
    // |task.delayed_run_time|, it does not alter sort order when it is called.
    void SetScheduled();

   private:
    bool scheduled_ = false;
    DISALLOW_COPY_AND_ASSIGN(DelayedTask);
  };

  // Pop and post all the ripe tasks in the delayed task queue.
  void ProcessRipeTasks();

  // Get the time at which to schedule the next |ProcessRipeTasks()| execution,
  // or TimeTicks::Max() if none needs to be scheduled (i.e. no task, or next
  // task already scheduled).
  TimeTicks GetTimeToScheduleProcessRipeTasksLockRequired();

  // Schedule |ProcessRipeTasks()| on the service thread to be executed at the
  // given |process_ripe_tasks_time|, provided the given time is not
  // TimeTicks::Max().
  void ScheduleProcessRipeTasksOnServiceThread(
      TimeTicks process_ripe_tasks_time);

  const RepeatingClosure process_ripe_tasks_closure_;

  const std::unique_ptr<const TickClock> tick_clock_;

  scoped_refptr<TaskRunner> service_thread_task_runner_;

  std::priority_queue<DelayedTask,
                      std::vector<DelayedTask>,
                      std::greater<DelayedTask>>
      delayed_task_queue_;

  // Synchronizes access to |delayed_task_queue_| and the setting of
  // |service_thread_task_runner|. Once |service_thread_task_runner_| is set,
  // it is never modified. It is therefore safe to access
  // |service_thread_task_runner_| without synchronization once it is observed
  // that it is non-null.
  SchedulerLock queue_lock_;

  DISALLOW_COPY_AND_ASSIGN(DelayedTaskManager);
};

}  // namespace internal
}  // namespace base

#endif  // BASE_TASK_TASK_SCHEDULER_DELAYED_TASK_MANAGER_H_
