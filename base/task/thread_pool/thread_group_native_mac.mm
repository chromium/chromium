// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/thread_pool/thread_group_native_mac.h"

#include "base/check.h"
#include "base/task/thread_pool/task_tracker.h"

namespace base {
namespace internal {

ThreadGroupNativeMac::ThreadGroupNativeMac(
    ThreadPriority priority_hint,
    TrackedRef<TaskTracker> task_tracker,
    TrackedRef<Delegate> delegate,
    ThreadGroup* predecessor_thread_group)
    : ThreadGroupNative(std::move(task_tracker),
                        std::move(delegate),
                        predecessor_thread_group),
      priority_hint_(priority_hint) {
  // The thread group only support NORMAL or BACKGROUND priority.
  DCHECK(priority_hint_ == ThreadPriority::NORMAL ||
         priority_hint_ == ThreadPriority::BACKGROUND);
}

ThreadGroupNativeMac::~ThreadGroupNativeMac() {}

void ThreadGroupNativeMac::StartImpl() {
  dispatch_queue_attr_t attributes = dispatch_queue_attr_make_with_qos_class(
      DISPATCH_QUEUE_CONCURRENT,
      priority_hint_ == ThreadPriority::NORMAL ? QOS_CLASS_USER_INITIATED
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
    RunNextTaskSourceImpl();
  });
}

}  // namespace internal
}  // namespace base
