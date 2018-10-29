// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MESSAGE_LOOP_MESSAGE_LOOP_TASK_RUNNER_H_
#define BASE_MESSAGE_LOOP_MESSAGE_LOOP_TASK_RUNNER_H_

#include "base/atomicops.h"
#include "base/base_export.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/sequenced_task_source.h"
#include "base/pending_task.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/lock.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"

namespace base {
namespace internal {

// A SingleThreadTaskRunner which receives and queues tasks destined to its
// owning MessageLoop. It does not manage delays (i.e. tasks returned by
// TakeTask() might have a non-zero delay).
class BASE_EXPORT MessageLoopTaskRunner : public SingleThreadTaskRunner,
                                          public SequencedTaskSource {
 public:
  // Constructs a MessageLoopTaskRunner which will notify |task_source_observer|
  // about tasks it receives. |task_source_observer| will be bound to this
  // MessageLoopTaskRunner's lifetime. Ownership is required as opposed to a raw
  // pointer since MessageLoopTaskRunner impls tend to receive tasks beyond the
  // destination's lifetime. For the same reasons, |task_source_observer| needs
  // to support being invoked racily during shutdown.
  MessageLoopTaskRunner(
      std::unique_ptr<SequencedTaskSource::Observer> task_source_observer);

  // Initialize this MessageLoopTaskRunner on the current thread.
  void BindToCurrentThread();

  // Instructs this task runner to stop accepting tasks, this cannot be undone.
  // Note that the registered SequencedTaskSource::Observer may still racily
  // receive a few DidQueueTask() calls while the Shutdown() signal propagates
  // to other threads and it needs to support that.
  void Shutdown();

  // SingleThreadTaskRunner:
  bool PostDelayedTask(const Location& from_here,
                       OnceClosure task,
                       TimeDelta delay) override;
  bool PostNonNestableDelayedTask(const Location& from_here,
                                  OnceClosure task,
                                  TimeDelta delay) override;
  bool RunsTasksInCurrentSequence() const override;

  // SequencedTaskSource:
  PendingTask TakeTask() override;
  bool HasTasks() override;
  void InjectTask(OnceClosure task) override;

  // When this functionality is enabled, AddToIncomingQueue() will also add the
  // queue time to the task.
  void SetAddQueueTimeToTasks(bool enable);

 private:
  friend class RefCountedThreadSafe<MessageLoopTaskRunner>;
  ~MessageLoopTaskRunner() override;

  // Appends a task to the incoming queue. Thread-safe.
  // Returns true if the task was successfully added to the queue.
  bool AddToIncomingQueue(const Location& from_here,
                          OnceClosure task,
                          TimeDelta delay,
                          Nestable nestable);

  // ID of the thread |this| was created on.  Could be accessed on multiple
  // threads, protected by |valid_thread_id_lock_|.
  PlatformThreadId valid_thread_id_ = kInvalidThreadId;
  mutable Lock valid_thread_id_lock_;

  std::unique_ptr<SequencedTaskSource::Observer> task_source_observer_;

  // Tasks to be returned to TakeTask(). Reloaded from |incoming_queue_| when
  // it becomes empty.
  TaskQueue outgoing_queue_;

  // Synchronizes access to all members below this line.
  base::Lock incoming_queue_lock_;

  // An incoming queue of tasks that are acquired under a mutex for processing
  // on this instance's thread. These tasks have not yet been been pushed to
  // |outgoing_queue_|.
  TaskQueue incoming_queue_;

  // True if the |outgoing_queue_| is empty. Toggled under
  // |incoming_queue_lock_| when reloading the work queue so that
  // PostPendingTaskLockRequired() can tell, without accessing the thread unsafe
  // |outgoing_queue_|, if this SequencedTaskSource has been made non-empty by a
  // PostTask() (and needs to inform its Observer).
  bool outgoing_queue_empty_ = true;

  // True if new tasks should be accepted.
  bool accept_new_tasks_ = true;

  // The next sequence number to use for delayed tasks.
  int next_sequence_num_ = 0;

  // Whether to add the queue time to tasks.
  base::subtle::Atomic32 add_queue_time_to_tasks_ = 0;

  DISALLOW_COPY_AND_ASSIGN(MessageLoopTaskRunner);
};

}  // namespace internal
}  // namespace base

#endif  // BASE_MESSAGE_LOOP_MESSAGE_LOOP_TASK_RUNNER_H_
