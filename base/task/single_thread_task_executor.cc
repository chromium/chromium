// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/single_thread_task_executor.h"

#include "base/task/sequence_manager/sequence_manager.h"
#include "base/task/sequence_manager/sequence_manager_impl.h"
#include "build/build_config.h"

namespace base {

SingleThreadTaskExecutor::SingleThreadTaskExecutor(MessagePumpType type)
    : sequence_manager_(sequence_manager::CreateUnboundSequenceManager(
          sequence_manager::SequenceManager::Settings::Builder()
              .SetMessagePumpType(type)
              .Build())),
      default_task_queue_(sequence_manager_->CreateTaskQueue(
          sequence_manager::TaskQueue::Spec("default_tq"))),
      type_(type),
      simple_task_executor_(sequence_manager_.get(), task_runner()) {
  sequence_manager_->SetDefaultTaskRunner(default_task_queue_->task_runner());
  sequence_manager_->BindToMessagePump(MessagePump::Create(type));

#if defined(OS_IOS)
  if (type == MessagePumpType::UI) {
    static_cast<sequence_manager::internal::SequenceManagerImpl*>(
        sequence_manager_.get())
        ->AttachToMessagePump();
  }
#endif
}

SingleThreadTaskExecutor::~SingleThreadTaskExecutor() = default;

scoped_refptr<SingleThreadTaskRunner> SingleThreadTaskExecutor::task_runner()
    const {
  return default_task_queue_->task_runner();
}

}  // namespace base
