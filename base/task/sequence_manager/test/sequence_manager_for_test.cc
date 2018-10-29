// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/sequence_manager/test/sequence_manager_for_test.h"

#include "base/task/sequence_manager/thread_controller_impl.h"

namespace base {
namespace sequence_manager {

namespace {

class ThreadControllerForTest : public internal::ThreadControllerImpl {
 public:
  ThreadControllerForTest(MessageLoop* message_loop,
                          scoped_refptr<SingleThreadTaskRunner> task_runner,
                          const TickClock* time_source)
      : ThreadControllerImpl(message_loop,
                             std::move(task_runner),
                             time_source) {}

  void AddNestingObserver(RunLoop::NestingObserver* observer) override {
    if (!message_loop_)
      return;
    ThreadControllerImpl::AddNestingObserver(observer);
  }

  void RemoveNestingObserver(RunLoop::NestingObserver* observer) override {
    if (!message_loop_)
      return;
    ThreadControllerImpl::RemoveNestingObserver(observer);
  }

  ~ThreadControllerForTest() override = default;
};

}  // namespace

SequenceManagerForTest::SequenceManagerForTest(
    std::unique_ptr<internal::ThreadController> thread_controller)
    : SequenceManagerImpl(std::move(thread_controller)) {}

// static
std::unique_ptr<SequenceManagerForTest> SequenceManagerForTest::Create(
    MessageLoop* message_loop,
    scoped_refptr<SingleThreadTaskRunner> task_runner,
    const TickClock* clock) {
  std::unique_ptr<SequenceManagerForTest> manager(
      new SequenceManagerForTest(std::make_unique<ThreadControllerForTest>(
          message_loop, std::move(task_runner), clock)));
  manager->BindToCurrentThread();
  manager->CompleteInitializationOnBoundThread();
  return manager;
}

// static
std::unique_ptr<SequenceManagerForTest> SequenceManagerForTest::Create(
    std::unique_ptr<internal::ThreadController> thread_controller) {
  std::unique_ptr<SequenceManagerForTest> manager(
      new SequenceManagerForTest(std::move(thread_controller)));
  manager->BindToCurrentThread();
  manager->CompleteInitializationOnBoundThread();
  return manager;
}

size_t SequenceManagerForTest::ActiveQueuesCount() const {
  return main_thread_only().active_queues.size();
}

bool SequenceManagerForTest::HasImmediateWork() const {
  return !main_thread_only().selector.AllEnabledWorkQueuesAreEmpty();
}

size_t SequenceManagerForTest::PendingTasksCount() const {
  size_t task_count = 0;
  for (auto* const queue : main_thread_only().active_queues)
    task_count += queue->GetNumberOfPendingTasks();
  return task_count;
}

size_t SequenceManagerForTest::QueuesToDeleteCount() const {
  return main_thread_only().queues_to_delete.size();
}

size_t SequenceManagerForTest::QueuesToShutdownCount() {
  return main_thread_only().queues_to_gracefully_shutdown.size();
}

}  // namespace sequence_manager
}  // namespace base
