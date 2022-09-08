// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/thread_pool/thread_group_native_mac.h"
#include "base/files/file_descriptor_watcher_posix.h"

#include "base/check.h"
#include "base/task/thread_pool/task_tracker.h"

namespace base {
namespace internal {

ThreadGroupNativeMac::ThreadGroupNativeMac(
    ThreadType thread_type_hint,
    scoped_refptr<SingleThreadTaskRunner> io_thread_task_runner,
    TrackedRef<TaskTracker> task_tracker,
    TrackedRef<Delegate> delegate,
    ThreadGroup* predecessor_thread_group)
    : ThreadGroupNative(std::move(task_tracker),
                        std::move(delegate),
                        predecessor_thread_group),
      thread_type_hint_(thread_type_hint),
      io_thread_task_runner_(std::move(io_thread_task_runner)) {
  // The thread group only support kNormal or kBackground type.
  DCHECK(thread_type_hint_ == ThreadType::kDefault ||
         thread_type_hint_ == ThreadType::kBackground);
  DCHECK(io_thread_task_runner_);
}

ThreadGroupNativeMac::~ThreadGroupNativeMac() {}

void ThreadGroupNativeMac::StartImpl() {
  dispatch_queue_attr_t attributes = dispatch_queue_attr_make_with_qos_class(
      DISPATCH_QUEUE_CONCURRENT,
      thread_type_hint_ == ThreadType::kDefault ? QOS_CLASS_USER_INITIATED
                                                : QOS_CLASS_BACKGROUND,
      /*relative_priority=*/-1);
  queue_.reset(dispatch_queue_create("org.chromium.base.ThreadPool.ThreadGroup",
                                     attributes));
  group_.reset(dispatch_group_create());
}

void ThreadGroupNativeMac::JoinImpl() {
  dispatch_group_wait(group_, DISPATCH_TIME_FOREVER);
}

void ThreadGroupNativeMac::SubmitWork() {
  dispatch_group_async(group_, queue_, ^{
    FileDescriptorWatcher file_descriptor_watcher(io_thread_task_runner_);
    RunNextTaskSourceImpl();
  });
}

}  // namespace internal
}  // namespace base
