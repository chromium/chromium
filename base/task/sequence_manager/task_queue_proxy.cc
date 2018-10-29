// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/sequence_manager/task_queue_proxy.h"

#include "base/task/sequence_manager/associated_thread_id.h"
#include "base/task/sequence_manager/sequence_manager_impl.h"
#include "base/task/sequence_manager/task_queue_impl.h"

namespace base {
namespace sequence_manager {
namespace internal {

TaskQueueProxy::TaskQueueProxy(
    TaskQueueImpl* task_queue_impl,
    scoped_refptr<AssociatedThreadId> associated_thread)
    : task_queue_impl_(task_queue_impl),
      associated_thread_(std::move(associated_thread)) {}

TaskQueueProxy::~TaskQueueProxy() = default;

bool TaskQueueProxy::PostTask(PostedTask task) const {
  // NOTE: Task's destructor might attempt to post another task,
  // so ensure it never happens inside this lock.
  Optional<MoveableAutoLock> lock(AcquireLockIfNeeded());
  if (!task_queue_impl_)
    return false;
  task_queue_impl_->PostTask(std::move(task));
  return true;
}

bool TaskQueueProxy::RunsTasksInCurrentSequence() const {
  return associated_thread_->thread_id == PlatformThread::CurrentId();
}

Optional<MoveableAutoLock> TaskQueueProxy::AcquireLockIfNeeded() const {
  if (RunsTasksInCurrentSequence())
    return nullopt;
  return MoveableAutoLock(lock_);
}

void TaskQueueProxy::DetachFromTaskQueueImpl() {
  DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);
  // |task_queue_impl_| can be read from the main thread without a lock,
  // but a lock is needed when we write to it.
  AutoLock lock(lock_);
  task_queue_impl_ = nullptr;
}

}  // namespace internal
}  // namespace sequence_manager
}  // namespace base
