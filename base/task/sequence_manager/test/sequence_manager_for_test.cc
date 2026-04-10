// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/sequence_manager/test/sequence_manager_for_test.h"

#include "base/task/sequence_manager/thread_controller_with_message_pump_impl.h"

namespace base::sequence_manager {

SequenceManagerForTest::SequenceManagerForTest(
    std::unique_ptr<internal::ThreadController> thread_controller,
    SequenceManager::Settings settings)
    : SequenceManagerImpl(std::move(thread_controller), std::move(settings)) {}

// static
std::unique_ptr<SequenceManagerForTest>
SequenceManagerForTest::CreateOnCurrentThread(
    std::unique_ptr<MessagePump> message_pump,
    SequenceManager::Settings settings) {
  auto thread_controller =
      std::make_unique<internal::ThreadControllerWithMessagePumpImpl>(
          std::move(message_pump), settings);
  std::unique_ptr<SequenceManagerForTest> manager(new SequenceManagerForTest(
      std::move(thread_controller), std::move(settings)));
  manager->BindToCurrentThread();
  return manager;
}

size_t SequenceManagerForTest::ActiveQueuesCount() const {
  return main_thread_only().active_queues.size();
}

bool SequenceManagerForTest::HasImmediateWork() const {
  return main_thread_only().selector.GetHighestPendingPriority().has_value();
}

size_t SequenceManagerForTest::PendingTasksCount() const {
  size_t task_count = 0;
  for (internal::TaskQueueImpl* const queue :
       main_thread_only().active_queues) {
    task_count += queue->GetNumberOfPendingTasks();
  }
  return task_count;
}

size_t SequenceManagerForTest::QueuesToDeleteCount() const {
  return main_thread_only().queues_to_delete.size();
}

}  // namespace base::sequence_manager
