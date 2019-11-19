// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/thread_pool/delayed_task_manager.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/thread_pool/task.h"
#include "base/test/bind_test_util.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace internal {
namespace {

constexpr TimeDelta kLongDelay = TimeDelta::FromHours(1);

class MockTask {
 public:
  MOCK_METHOD0(Run, void());
};

void RunTask(Task task) {
  std::move(task.task).Run();
}

Task ConstructMockedTask(testing::StrictMock<MockTask>& mock_task,
                         TimeTicks now,
                         TimeDelta delay) {
  Task task(FROM_HERE, BindOnce(&MockTask::Run, Unretained(&mock_task)), delay);
  // The constructor of Task computes |delayed_run_time| by adding |delay| to
  // the real time. Recompute it by adding |delay| to the given |now| (usually
  // mock time).
  task.delayed_run_time = now + delay;
  return task;
}

class ThreadPoolDelayedTaskManagerTest : public testing::Test {
 protected:
  ThreadPoolDelayedTaskManagerTest() = default;
  ~ThreadPoolDelayedTaskManagerTest() override = default;

  const scoped_refptr<TestMockTimeTaskRunner> service_thread_task_runner_ =
      MakeRefCounted<TestMockTimeTaskRunner>();
  DelayedTaskManager delayed_task_manager_{
      service_thread_task_runner_->GetMockTickClock()};
  testing::StrictMock<MockTask> mock_task_;
  Task task_{ConstructMockedTask(mock_task_,
                                 service_thread_task_runner_->NowTicks(),
                                 kLongDelay)};

 private:
  DISALLOW_COPY_AND_ASSIGN(ThreadPoolDelayedTaskManagerTest);
};

}  // namespace

// Verify that a delayed task isn't forwarded before Start().
TEST_F(ThreadPoolDelayedTaskManagerTest, DelayedTaskDoesNotRunBeforeStart) {
  // Send |task| to the DelayedTaskManager.
  delayed_task_manager_.AddDelayedTask(std::move(task_), BindOnce(&RunTask),
                                       nullptr);

  // Fast-forward time until the task is ripe for execution. Since Start() has
  // not been called, the task should not be forwarded to RunTask() (MockTask is
  // a StrictMock without expectations so test will fail if RunTask() runs it).
  service_thread_task_runner_->FastForwardBy(kLongDelay);
}

// Verify that a delayed task added before Start() and whose delay expires after
// Start() is forwarded when its delay expires.
TEST_F(ThreadPoolDelayedTaskManagerTest,
       DelayedTaskPostedBeforeStartExpiresAfterStartRunsOnExpire) {
  // Send |task| to the DelayedTaskManager.
  delayed_task_manager_.AddDelayedTask(std::move(task_), BindOnce(&RunTask),
                                       nullptr);

  delayed_task_manager_.Start(service_thread_task_runner_);

  // Run tasks on the service thread. Don't expect any forwarding to
  // |task_target_| since the task isn't ripe for execution.
  service_thread_task_runner_->RunUntilIdle();

  // Fast-forward time until the task is ripe for execution. Expect the task to
  // be forwarded to RunTask().
  EXPECT_CALL(mock_task_, Run());
  service_thread_task_runner_->FastForwardBy(kLongDelay);
}

// Verify that a delayed task added before Start() and whose delay expires
// before Start() is forwarded when Start() is called.
TEST_F(ThreadPoolDelayedTaskManagerTest,
       DelayedTaskPostedBeforeStartExpiresBeforeStartRunsOnStart) {
  // Send |task| to the DelayedTaskManager.
  delayed_task_manager_.AddDelayedTask(std::move(task_), BindOnce(&RunTask),
                                       nullptr);

  // Run tasks on the service thread. Don't expect any forwarding to
  // |task_target_| since the task isn't ripe for execution.
  service_thread_task_runner_->RunUntilIdle();

  // Fast-forward time until the task is ripe for execution. Don't expect the
  // task to be forwarded since Start() hasn't been called yet.
  service_thread_task_runner_->FastForwardBy(kLongDelay);

  // Start the DelayedTaskManager. Expect the task to be forwarded to RunTask().
  EXPECT_CALL(mock_task_, Run());
  delayed_task_manager_.Start(service_thread_task_runner_);
  service_thread_task_runner_->RunUntilIdle();
}

// Verify that a delayed task added after Start() isn't forwarded before it is
// ripe for execution.
TEST_F(ThreadPoolDelayedTaskManagerTest, DelayedTaskDoesNotRunTooEarly) {
  delayed_task_manager_.Start(service_thread_task_runner_);

  // Send |task| to the DelayedTaskManager.
  delayed_task_manager_.AddDelayedTask(std::move(task_), BindOnce(&RunTask),
                                       nullptr);

  // Run tasks that are ripe for execution. Don't expect any forwarding to
  // RunTask().
  service_thread_task_runner_->RunUntilIdle();
}

// Verify that a delayed task added after Start() is forwarded when it is ripe
// for execution.
TEST_F(ThreadPoolDelayedTaskManagerTest, DelayedTaskRunsAfterDelay) {
  delayed_task_manager_.Start(service_thread_task_runner_);

  // Send |task| to the DelayedTaskManager.
  delayed_task_manager_.AddDelayedTask(std::move(task_), BindOnce(&RunTask),
                                       nullptr);

  // Fast-forward time. Expect the task to be forwarded to RunTask().
  EXPECT_CALL(mock_task_, Run());
  service_thread_task_runner_->FastForwardBy(kLongDelay);
}

// Verify that multiple delayed tasks added after Start() are forwarded when
// they are ripe for execution.
TEST_F(ThreadPoolDelayedTaskManagerTest, DelayedTasksRunAfterDelay) {
  delayed_task_manager_.Start(service_thread_task_runner_);

  testing::StrictMock<MockTask> mock_task_a;
  Task task_a =
      ConstructMockedTask(mock_task_a, service_thread_task_runner_->NowTicks(),
                          TimeDelta::FromHours(1));

  testing::StrictMock<MockTask> mock_task_b;
  Task task_b =
      ConstructMockedTask(mock_task_b, service_thread_task_runner_->NowTicks(),
                          TimeDelta::FromHours(2));

  testing::StrictMock<MockTask> mock_task_c;
  Task task_c =
      ConstructMockedTask(mock_task_c, service_thread_task_runner_->NowTicks(),
                          TimeDelta::FromHours(1));

  // Send tasks to the DelayedTaskManager.
  delayed_task_manager_.AddDelayedTask(std::move(task_a), BindOnce(&RunTask),
                                       nullptr);
  delayed_task_manager_.AddDelayedTask(std::move(task_b), BindOnce(&RunTask),
                                       nullptr);
  delayed_task_manager_.AddDelayedTask(std::move(task_c), BindOnce(&RunTask),
                                       nullptr);

  // Run tasks that are ripe for execution on the service thread. Don't expect
  // any call to RunTask().
  service_thread_task_runner_->RunUntilIdle();

  // Fast-forward time. Expect |task_a| and |task_c| to be forwarded to
  // |task_target_|.
  EXPECT_CALL(mock_task_a, Run());
  EXPECT_CALL(mock_task_c, Run());
  service_thread_task_runner_->FastForwardBy(TimeDelta::FromHours(1));
  testing::Mock::VerifyAndClear(&mock_task_a);
  testing::Mock::VerifyAndClear(&mock_task_c);

  // Fast-forward time. Expect |task_b| to be forwarded to RunTask().
  EXPECT_CALL(mock_task_b, Run());
  service_thread_task_runner_->FastForwardBy(TimeDelta::FromHours(1));
  testing::Mock::VerifyAndClear(&mock_task_b);
}

TEST_F(ThreadPoolDelayedTaskManagerTest, PostTaskDuringStart) {
  Thread other_thread("Test");
  other_thread.StartAndWaitForTesting();

  WaitableEvent task_posted;

  other_thread.task_runner()->PostTask(
      FROM_HERE, BindLambdaForTesting([&]() {
        delayed_task_manager_.AddDelayedTask(
            std::move(task_), BindOnce(&RunTask), other_thread.task_runner());
        task_posted.Signal();
      }));

  delayed_task_manager_.Start(service_thread_task_runner_);

  // The test is testing a race between AddDelayedTask/Start but it still needs
  // synchronization to ensure we don't do the final verification before the
  // task itself is posted.
  task_posted.Wait();

  // Fast-forward time. Expect the task to be forwarded to RunTask().
  EXPECT_CALL(mock_task_, Run());
  service_thread_task_runner_->FastForwardBy(kLongDelay);
}

}  // namespace internal
}  // namespace base
