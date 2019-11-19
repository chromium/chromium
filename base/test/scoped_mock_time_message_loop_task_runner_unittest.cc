// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_mock_time_message_loop_task_runner.h"

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_forward.h"
#include "base/containers/circular_deque.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_current.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/test/test_pending_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace {

TaskRunner* GetCurrentTaskRunner() {
  return ThreadTaskRunnerHandle::Get().get();
}

void AssignTrue(bool* out) {
  *out = true;
}

// Pops a task from the front of |pending_tasks| and returns it.
TestPendingTask PopFront(base::circular_deque<TestPendingTask>* pending_tasks) {
  TestPendingTask task = std::move(pending_tasks->front());
  pending_tasks->pop_front();
  return task;
}

class ScopedMockTimeMessageLoopTaskRunnerTest : public testing::Test {
 public:
  ScopedMockTimeMessageLoopTaskRunnerTest()
      : original_task_runner_(new TestMockTimeTaskRunner()) {
    MessageLoopCurrent::Get()->SetTaskRunner(original_task_runner_);
  }

 protected:
  TestMockTimeTaskRunner* original_task_runner() {
    return original_task_runner_.get();
  }

 private:
  scoped_refptr<TestMockTimeTaskRunner> original_task_runner_;

  MessageLoop message_loop_;

  DISALLOW_COPY_AND_ASSIGN(ScopedMockTimeMessageLoopTaskRunnerTest);
};

// Verifies a new TaskRunner is installed while a
// ScopedMockTimeMessageLoopTaskRunner exists and the previous one is installed
// after destruction.
TEST_F(ScopedMockTimeMessageLoopTaskRunnerTest, CurrentTaskRunners) {
  auto scoped_task_runner_ =
      std::make_unique<ScopedMockTimeMessageLoopTaskRunner>();
  EXPECT_EQ(scoped_task_runner_->task_runner(), GetCurrentTaskRunner());
  scoped_task_runner_.reset();
  EXPECT_EQ(original_task_runner(), GetCurrentTaskRunner());
}

TEST_F(ScopedMockTimeMessageLoopTaskRunnerTest,
       IncompleteTasksAreCopiedToPreviousTaskRunnerAfterDestruction) {
  auto scoped_task_runner_ =
      std::make_unique<ScopedMockTimeMessageLoopTaskRunner>();

  bool task_10_has_run = false;
  bool task_11_has_run = false;

  OnceClosure task_1 = DoNothing();
  OnceClosure task_2 = DoNothing();
  OnceClosure task_10 = BindOnce(&AssignTrue, &task_10_has_run);
  OnceClosure task_11 = BindOnce(&AssignTrue, &task_11_has_run);

  constexpr TimeDelta task_1_delay = TimeDelta::FromSeconds(1);
  constexpr TimeDelta task_2_delay = TimeDelta::FromSeconds(2);
  constexpr TimeDelta task_10_delay = TimeDelta::FromSeconds(10);
  constexpr TimeDelta task_11_delay = TimeDelta::FromSeconds(11);

  constexpr TimeDelta step_time_by = TimeDelta::FromSeconds(5);

  GetCurrentTaskRunner()->PostDelayedTask(FROM_HERE, std::move(task_1),
                                          task_1_delay);
  GetCurrentTaskRunner()->PostDelayedTask(FROM_HERE, std::move(task_2),
                                          task_2_delay);
  GetCurrentTaskRunner()->PostDelayedTask(FROM_HERE, std::move(task_10),
                                          task_10_delay);
  GetCurrentTaskRunner()->PostDelayedTask(FROM_HERE, std::move(task_11),
                                          task_11_delay);

  scoped_task_runner_->task_runner()->FastForwardBy(step_time_by);

  scoped_task_runner_.reset();

  base::circular_deque<TestPendingTask> pending_tasks =
      original_task_runner()->TakePendingTasks();

  EXPECT_EQ(2U, pending_tasks.size());

  TestPendingTask pending_task = PopFront(&pending_tasks);
  EXPECT_FALSE(task_10_has_run);
  std::move(pending_task.task).Run();
  EXPECT_TRUE(task_10_has_run);
  EXPECT_EQ(task_10_delay - step_time_by, pending_task.delay);

  pending_task = PopFront(&pending_tasks);
  EXPECT_FALSE(task_11_has_run);
  std::move(pending_task.task).Run();
  EXPECT_TRUE(task_11_has_run);
  EXPECT_EQ(task_11_delay - step_time_by, pending_task.delay);
}

}  // namespace
}  // namespace base
