// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/thread_pool/delayed_task_manager.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/cancelable_callback.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/thread_pool/task.h"
#include "base/test/bind.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace internal {
namespace {

constexpr TimeDelta kLongerDelay = TimeDelta::FromHours(3);
constexpr TimeDelta kLongDelay = TimeDelta::FromHours(1);

class MockCallback {
 public:
  MOCK_METHOD0(Run, void());
};

void PostTaskNow(Task task) {
  std::move(task.task).Run();
}

Task ConstructMockedTask(testing::StrictMock<MockCallback>& mock_task,
                         TimeTicks now,
                         TimeDelta delay) {
  Task task(FROM_HERE, BindOnce(&MockCallback::Run, Unretained(&mock_task)),
            now, delay);
  return task;
}

class ThreadPoolDelayedTaskManagerTest : public testing::Test {
 public:
  ThreadPoolDelayedTaskManagerTest(const ThreadPoolDelayedTaskManagerTest&) =
      delete;
  ThreadPoolDelayedTaskManagerTest& operator=(
      const ThreadPoolDelayedTaskManagerTest&) = delete;

 protected:
  ThreadPoolDelayedTaskManagerTest() = default;
  ~ThreadPoolDelayedTaskManagerTest() override = default;

  const scoped_refptr<TestMockTimeTaskRunner> service_thread_task_runner_ =
      MakeRefCounted<TestMockTimeTaskRunner>();
  DelayedTaskManager delayed_task_manager_{
      service_thread_task_runner_->GetMockTickClock()};
  testing::StrictMock<MockCallback> mock_callback_;
  Task task_{ConstructMockedTask(mock_callback_,
                                 service_thread_task_runner_->NowTicks(),
                                 kLongDelay)};
};

}  // namespace

// Verify that a delayed task isn't forwarded before Start().
TEST_F(ThreadPoolDelayedTaskManagerTest, DelayedTaskDoesNotRunBeforeStart) {
  // Send |task| to the DelayedTaskManager.
  delayed_task_manager_.AddDelayedTask(std::move(task_), BindOnce(&PostTaskNow),
                                       nullptr);

  // Fast-forward time until the task is ripe for execution. Since Start() has
  // not been called, the task should not be forwarded to PostTaskNow()
  // (MockCallback is a StrictMock without expectations so test will fail if
  // PostTaskNow() runs it).
  service_thread_task_runner_->FastForwardBy(kLongDelay);
}

// Verify that a delayed task added before Start() and whose delay expires after
// Start() is forwarded when its delay expires.
TEST_F(ThreadPoolDelayedTaskManagerTest,
       DelayedTaskPostedBeforeStartExpiresAfterStartRunsOnExpire) {
  // Send |task| to the DelayedTaskManager.
  delayed_task_manager_.AddDelayedTask(std::move(task_), BindOnce(&PostTaskNow),
                                       nullptr);

  delayed_task_manager_.Start(service_thread_task_runner_);

  // Run tasks on the service thread. Don't expect any forwarding to
  // |task_target_| since the task isn't ripe for execution.
  service_thread_task_runner_->RunUntilIdle();

  // Fast-forward time until the task is ripe for execution. Expect the task to
  // be forwarded to PostTaskNow().
  EXPECT_CALL(mock_callback_, Run());
  service_thread_task_runner_->FastForwardBy(kLongDelay);
}

// Verify that a delayed task added before Start() and whose delay expires
// before Start() is forwarded when Start() is called.
TEST_F(ThreadPoolDelayedTaskManagerTest,
       DelayedTaskPostedBeforeStartExpiresBeforeStartRunsOnStart) {
  // Send |task| to the DelayedTaskManager.
  delayed_task_manager_.AddDelayedTask(std::move(task_), BindOnce(&PostTaskNow),
                                       nullptr);

  // Run tasks on the service thread. Don't expect any forwarding to
  // |task_target_| since the task isn't ripe for execution.
  service_thread_task_runner_->RunUntilIdle();

  // Fast-forward time until the task is ripe for execution. Don't expect the
  // task to be forwarded since Start() hasn't been called yet.
  service_thread_task_runner_->FastForwardBy(kLongDelay);

  // Start the DelayedTaskManager. Expect the task to be forwarded to
  // PostTaskNow().
  EXPECT_CALL(mock_callback_, Run());
  delayed_task_manager_.Start(service_thread_task_runner_);
  service_thread_task_runner_->RunUntilIdle();
}

// Verify that a delayed task added after Start() isn't forwarded before it is
// ripe for execution.
TEST_F(ThreadPoolDelayedTaskManagerTest, DelayedTaskDoesNotRunTooEarly) {
  delayed_task_manager_.Start(service_thread_task_runner_);

  // Send |task| to the DelayedTaskManager.
  delayed_task_manager_.AddDelayedTask(std::move(task_), BindOnce(&PostTaskNow),
                                       nullptr);

  // Run tasks that are ripe for execution. Don't expect any forwarding to
  // PostTaskNow().
  service_thread_task_runner_->RunUntilIdle();
}

// Verify that a delayed task added after Start() is forwarded when it is ripe
// for execution.
TEST_F(ThreadPoolDelayedTaskManagerTest, DelayedTaskRunsAfterDelay) {
  delayed_task_manager_.Start(service_thread_task_runner_);

  // Send |task| to the DelayedTaskManager.
  delayed_task_manager_.AddDelayedTask(std::move(task_), BindOnce(&PostTaskNow),
                                       nullptr);

  // Fast-forward time. Expect the task to be forwarded to PostTaskNow().
  EXPECT_CALL(mock_callback_, Run());
  service_thread_task_runner_->FastForwardBy(kLongDelay);
}

// Verify that a delayed task added after Start() is forwarded when it is
// canceled, even if its delay hasn't expired.
TEST_F(ThreadPoolDelayedTaskManagerTest, DelayedTaskRunsAfterCancelled) {
  static_assert(kLongerDelay > kLongDelay, "");

  delayed_task_manager_.Start(service_thread_task_runner_);

  // Add a cancelable task to the DelayedTaskManager with a longer delay.
  CancelableOnceClosure cancelable_closure(DoNothing::Once());
  bool post_cancelable_task_now_invoked = false;
  Task cancelable_task(FROM_HERE, cancelable_closure.callback(),
                       TimeTicks::Now(), kLongerDelay);
  auto post_cancelable_task_now = BindLambdaForTesting(
      [&](Task task) { post_cancelable_task_now_invoked = true; });
  delayed_task_manager_.AddDelayedTask(std::move(cancelable_task),
                                       post_cancelable_task_now, nullptr);

  // Add |task_| to the DelayedTaskManager with a long delay.
  delayed_task_manager_.AddDelayedTask(std::move(task_), BindOnce(&PostTaskNow),
                                       nullptr);

  // Cancel the cancelable task.
  cancelable_closure.Cancel();

  // Fast-forward time by |kLongDelay|. The non-cancelable task should have its
  // "post task now" callback invoked and it should run. The canceled task
  // should have its "post task now" callback invoked, even if its delay hasn't
  // expired.
  EXPECT_CALL(mock_callback_, Run());
  service_thread_task_runner_->FastForwardBy(kLongDelay);
  EXPECT_TRUE(post_cancelable_task_now_invoked);
}

// Verify that multiple delayed tasks added after Start() are forwarded when
// they are ripe for execution.
TEST_F(ThreadPoolDelayedTaskManagerTest, DelayedTasksRunAfterDelay) {
  delayed_task_manager_.Start(service_thread_task_runner_);

  testing::StrictMock<MockCallback> mock_callback_a;
  Task task_a = ConstructMockedTask(mock_callback_a,
                                    service_thread_task_runner_->NowTicks(),
                                    TimeDelta::FromHours(1));

  testing::StrictMock<MockCallback> mock_callback_b;
  Task task_b = ConstructMockedTask(mock_callback_b,
                                    service_thread_task_runner_->NowTicks(),
                                    TimeDelta::FromHours(2));

  testing::StrictMock<MockCallback> mock_callback_c;
  Task task_c = ConstructMockedTask(mock_callback_c,
                                    service_thread_task_runner_->NowTicks(),
                                    TimeDelta::FromHours(1));

  // Send tasks to the DelayedTaskManager.
  delayed_task_manager_.AddDelayedTask(std::move(task_a),
                                       BindOnce(&PostTaskNow), nullptr);
  delayed_task_manager_.AddDelayedTask(std::move(task_b),
                                       BindOnce(&PostTaskNow), nullptr);
  delayed_task_manager_.AddDelayedTask(std::move(task_c),
                                       BindOnce(&PostTaskNow), nullptr);

  // Run tasks that are ripe for execution on the service thread. Don't expect
  // any call to PostTaskNow().
  service_thread_task_runner_->RunUntilIdle();

  // Fast-forward time. Expect |task_a| and |task_c| to be forwarded to
  // |task_target_|.
  EXPECT_CALL(mock_callback_a, Run());
  EXPECT_CALL(mock_callback_c, Run());
  service_thread_task_runner_->FastForwardBy(TimeDelta::FromHours(1));
  testing::Mock::VerifyAndClear(&mock_callback_a);
  testing::Mock::VerifyAndClear(&mock_callback_c);

  // Fast-forward time. Expect |task_b| to be forwarded to PostTaskNow().
  EXPECT_CALL(mock_callback_b, Run());
  service_thread_task_runner_->FastForwardBy(TimeDelta::FromHours(1));
  testing::Mock::VerifyAndClear(&mock_callback_b);
}

TEST_F(ThreadPoolDelayedTaskManagerTest, PostTaskDuringStart) {
  Thread other_thread("Test");
  other_thread.StartAndWaitForTesting();

  WaitableEvent task_posted;

  other_thread.task_runner()->PostTask(FROM_HERE, BindLambdaForTesting([&]() {
                                         delayed_task_manager_.AddDelayedTask(
                                             std::move(task_),
                                             BindOnce(&PostTaskNow),
                                             other_thread.task_runner());
                                         task_posted.Signal();
                                       }));

  delayed_task_manager_.Start(service_thread_task_runner_);

  // The test is testing a race between AddDelayedTask/Start but it still needs
  // synchronization to ensure we don't do the final verification before the
  // task itself is posted.
  task_posted.Wait();

  // Fast-forward time. Expect the task to be forwarded to PostTaskNow().
  EXPECT_CALL(mock_callback_, Run());
  service_thread_task_runner_->FastForwardBy(kLongDelay);
}

}  // namespace internal
}  // namespace base
