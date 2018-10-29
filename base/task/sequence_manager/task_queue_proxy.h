// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_SEQUENCE_MANAGER_TASK_QUEUE_PROXY_H_
#define BASE_TASK_SEQUENCE_MANAGER_TASK_QUEUE_PROXY_H_

#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "base/synchronization/lock.h"
#include "base/task/sequence_manager/moveable_auto_lock.h"
#include "base/task/sequence_manager/task_queue.h"
#include "base/task/sequence_manager/tasks.h"

namespace base {
namespace sequence_manager {
namespace internal {

struct AssociatedThreadId;
class TaskQueueImpl;

// Task runners are ref-counted and unaccountable, so we need a safe way
// to delete a task queue while associated task runners might be still around.
// When TaskQueueImpl goes away, this proxy becomes a stub and later on gets
// ref-count-destructed once no TaskQueueTaskRunner remains.
// NOTE: Instances must be constructed or detached only by TaskQueueImpl,
// unless |task_queue_impl| is null (which is useful for stub task runners).
class TaskQueueProxy : public RefCountedThreadSafe<TaskQueueProxy> {
 public:
  TaskQueueProxy(TaskQueueImpl* task_queue_impl,
                 scoped_refptr<AssociatedThreadId> associated_thread);

  // May be called on any thread.
  bool PostTask(PostedTask task) const;
  bool RunsTasksInCurrentSequence() const;

  // PostTask will reject any task after this call.
  // Must be called on main thread only.
  void DetachFromTaskQueueImpl();

 private:
  friend class RefCountedThreadSafe<TaskQueueProxy>;
  ~TaskQueueProxy();

  // Doesn't acquire lock on main thread.
  Optional<MoveableAutoLock> AcquireLockIfNeeded() const;

  mutable Lock lock_;
  TaskQueueImpl* task_queue_impl_;  // Not owned.
  const scoped_refptr<AssociatedThreadId> associated_thread_;
};

}  // namespace internal
}  // namespace sequence_manager
}  // namespace base

#endif  // BASE_TASK_SEQUENCE_MANAGER_TASK_QUEUE_PROXY_H_
