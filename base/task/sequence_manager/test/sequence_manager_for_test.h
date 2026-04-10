// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_SEQUENCE_MANAGER_TEST_SEQUENCE_MANAGER_FOR_TEST_H_
#define BASE_TASK_SEQUENCE_MANAGER_TEST_SEQUENCE_MANAGER_FOR_TEST_H_

#include <memory>

#include "base/task/sequence_manager/sequence_manager.h"
#include "base/task/sequence_manager/sequence_manager_impl.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/tick_clock.h"

namespace base {

namespace sequence_manager {

class SequenceManagerForTest : public internal::SequenceManagerImpl {
 public:
  ~SequenceManagerForTest() override = default;

  static std::unique_ptr<SequenceManagerForTest> CreateOnCurrentThread(
      std::unique_ptr<MessagePump> message_pump,
      SequenceManager::Settings);

  size_t ActiveQueuesCount() const;
  bool HasImmediateWork() const;
  size_t PendingTasksCount() const;
  size_t QueuesToDeleteCount() const;
  size_t QueuesToShutdownCount();

  using internal::SequenceManagerImpl::GetNextSequenceNumber;
  using internal::SequenceManagerImpl::MoveReadyDelayedTasksToWorkQueues;
  using internal::SequenceManagerImpl::ReloadEmptyWorkQueues;

 private:
  explicit SequenceManagerForTest(
      std::unique_ptr<internal::ThreadController> thread_controller,
      SequenceManager::Settings settings);
};

}  // namespace sequence_manager
}  // namespace base

#endif  // BASE_TASK_SEQUENCE_MANAGER_TEST_SEQUENCE_MANAGER_FOR_TEST_H_
