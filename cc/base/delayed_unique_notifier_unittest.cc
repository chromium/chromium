// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/base/delayed_unique_notifier.h"

#include <utility>

#include "base/containers/circular_deque.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/test_pending_task.h"
#include "base/test/test_simple_task_runner.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

class TestNotifier : public DelayedUniqueNotifier {
 public:
  TestNotifier(base::SequencedTaskRunner* task_runner,
               base::RepeatingClosure closure,
               const base::TimeDelta& delay)
      : DelayedUniqueNotifier(task_runner, std::move(closure), delay) {}
  ~TestNotifier() override = default;

  // Overridden from DelayedUniqueNotifier:
  base::TimeTicks Now() const override { return now_; }

  void SetNow(base::TimeTicks now) { now_ = now; }

 private:
  base::TimeTicks now_;
};

class DelayedUniqueNotifierTest : public testing::Test {
 public:
  DelayedUniqueNotifierTest() : notification_count_(0) {}

  void SetUp() override {
    notification_count_ = 0;
    task_runner_ = base::MakeRefCounted<base::TestSimpleTaskRunner>();
  }

  void Notify() { ++notification_count_; }

  int NotificationCount() const { return notification_count_; }

  base::circular_deque<base::TestPendingTask> TakePendingTasks() {
    return task_runner_->TakePendingTasks();
  }

 protected:
  int notification_count_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
};

TEST_F(DelayedUniqueNotifierTest, SmallDelay) {
  base::TimeDelta delay = base::Microseconds(20);
  TestNotifier notifier(task_runner_.get(),
                        base::BindRepeating(&DelayedUniqueNotifierTest::Notify,
                                            base::Unretained(this)),
                        delay);

  EXPECT_EQ(0, NotificationCount());

  // Basic schedule for |delay| from now (now: 30, run time: 50).
  base::TimeTicks schedule_time = base::TimeTicks() + base::Microseconds(30);

  notifier.SetNow(schedule_time);
  notifier.Schedule();

  base::circular_deque<base::TestPendingTask> tasks = TakePendingTasks();

  ASSERT_EQ(1u, tasks.size());
  EXPECT_EQ(base::TimeTicks() + delay, tasks[0].GetTimeToRun());

  // It's not yet time to run, so we expect no notifications.
  std::move(tasks[0].task).Run();
  EXPECT_EQ(0, NotificationCount());

  tasks = TakePendingTasks();

  ASSERT_EQ(1u, tasks.size());
  // Now the time should be delay minus whatever the value of now happens to be
  // (now: 30, run time: 50).
  base::TimeTicks scheduled_run_time = notifier.Now() + delay;
  base::TimeTicks scheduled_delay =
      base::TimeTicks() + (scheduled_run_time - notifier.Now());
  EXPECT_EQ(scheduled_delay, tasks[0].GetTimeToRun());

  // Move closer to the run time (time: 49, run time: 50).
  notifier.SetNow(notifier.Now() + base::Microseconds(19));

  // It's not yet time to run, so we expect no notifications.
  std::move(tasks[0].task).Run();
  EXPECT_EQ(0, NotificationCount());

  tasks = TakePendingTasks();
  ASSERT_EQ(1u, tasks.size());

  // Now the time should be delay minus whatever the value of now happens to be.
  scheduled_delay = base::TimeTicks() + (scheduled_run_time - notifier.Now());
  EXPECT_EQ(scheduled_delay, tasks[0].GetTimeToRun());

  // Move to exactly the run time (time: 50, run time: 50).
  notifier.SetNow(notifier.Now() + base::Microseconds(1));

  // It's time to run!
  std::move(tasks[0].task).Run();
  EXPECT_EQ(1, NotificationCount());

  tasks = TakePendingTasks();
  EXPECT_EQ(0u, tasks.size());
}

TEST_F(DelayedUniqueNotifierTest, RescheduleDelay) {
  base::TimeDelta delay = base::Microseconds(20);
  TestNotifier notifier(task_runner_.get(),
                        base::BindRepeating(&DelayedUniqueNotifierTest::Notify,
                                            base::Unretained(this)),
                        delay);

  base::TimeTicks schedule_time;
  // Move time 19 units forward and reschedule, expecting that we still need to
  // run in |delay| time and we don't get a notification.
  for (int i = 0; i < 10; ++i) {
    EXPECT_EQ(0, NotificationCount());

    // Move time forward 19 units.
    schedule_time = notifier.Now() + base::Microseconds(19);
    notifier.SetNow(schedule_time);
    notifier.Schedule();

    base::circular_deque<base::TestPendingTask> tasks = TakePendingTasks();

    ASSERT_EQ(1u, tasks.size());
    EXPECT_EQ(base::TimeTicks() + delay, tasks[0].GetTimeToRun());

    // It's not yet time to run, so we expect no notifications.
    std::move(tasks[0].task).Run();
    EXPECT_EQ(0, NotificationCount());
  }

  // Move time forward 20 units, expecting a notification.
  schedule_time = notifier.Now() + base::Microseconds(20);
  notifier.SetNow(schedule_time);

  base::circular_deque<base::TestPendingTask> tasks = TakePendingTasks();

  ASSERT_EQ(1u, tasks.size());
  EXPECT_EQ(base::TimeTicks() + delay, tasks[0].GetTimeToRun());

  // Time to run!
  std::move(tasks[0].task).Run();
  EXPECT_EQ(1, NotificationCount());
}

TEST_F(DelayedUniqueNotifierTest, CancelAndHasPendingNotification) {
  base::TimeDelta delay = base::Microseconds(20);
  TestNotifier notifier(task_runner_.get(),
                        base::BindRepeating(&DelayedUniqueNotifierTest::Notify,
                                            base::Unretained(this)),
                        delay);

  EXPECT_EQ(0, NotificationCount());

  // Schedule for |delay| seconds from now.
  base::TimeTicks schedule_time = notifier.Now() + base::Microseconds(10);
  notifier.SetNow(schedule_time);
  notifier.Schedule();
  EXPECT_TRUE(notifier.HasPendingNotification());

  // Cancel the run.
  notifier.Cancel();
  EXPECT_FALSE(notifier.HasPendingNotification());

  base::circular_deque<base::TestPendingTask> tasks = TakePendingTasks();

  ASSERT_EQ(1u, tasks.size());
  EXPECT_EQ(base::TimeTicks() + delay, tasks[0].GetTimeToRun());

  // Time to run, but a canceled task!
  std::move(tasks[0].task).Run();
  EXPECT_EQ(0, NotificationCount());
  EXPECT_FALSE(notifier.HasPendingNotification());

  tasks = TakePendingTasks();
  EXPECT_EQ(0u, tasks.size());

  notifier.Schedule();
  EXPECT_TRUE(notifier.HasPendingNotification());
  tasks = TakePendingTasks();

  ASSERT_EQ(1u, tasks.size());
  EXPECT_EQ(base::TimeTicks() + delay, tasks[0].GetTimeToRun());

  // Advance the time.
  notifier.SetNow(notifier.Now() + delay);

  // This should run since it wasn't canceled.
  std::move(tasks[0].task).Run();
  EXPECT_EQ(1, NotificationCount());
  EXPECT_FALSE(notifier.HasPendingNotification());

  for (int i = 0; i < 10; ++i) {
    notifier.Schedule();
    EXPECT_TRUE(notifier.HasPendingNotification());
    notifier.Cancel();
    EXPECT_FALSE(notifier.HasPendingNotification());
  }

  tasks = TakePendingTasks();

  ASSERT_EQ(1u, tasks.size());
  EXPECT_EQ(base::TimeTicks() + delay, tasks[0].GetTimeToRun());

  // Time to run, but a canceled task!
  notifier.SetNow(notifier.Now() + delay);
  std::move(tasks[0].task).Run();
  EXPECT_EQ(1, NotificationCount());

  tasks = TakePendingTasks();
  EXPECT_EQ(0u, tasks.size());
  EXPECT_FALSE(notifier.HasPendingNotification());
}

TEST_F(DelayedUniqueNotifierTest, ShutdownWithScheduledTask) {
  base::TimeDelta delay = base::Microseconds(20);
  TestNotifier notifier(task_runner_.get(),
                        base::BindRepeating(&DelayedUniqueNotifierTest::Notify,
                                            base::Unretained(this)),
                        delay);

  EXPECT_EQ(0, NotificationCount());

  // Schedule for |delay| seconds from now.
  base::TimeTicks schedule_time = notifier.Now() + base::Microseconds(10);
  notifier.SetNow(schedule_time);
  notifier.Schedule();
  EXPECT_TRUE(notifier.HasPendingNotification());

  // Shutdown the notifier.
  notifier.Shutdown();

  // The task is still there, but...
  base::circular_deque<base::TestPendingTask> tasks = TakePendingTasks();
  ASSERT_EQ(1u, tasks.size());

  // Running the task after shutdown does nothing since it's cancelled.
  std::move(tasks[0].task).Run();
  EXPECT_EQ(0, NotificationCount());

  tasks = TakePendingTasks();
  EXPECT_EQ(0u, tasks.size());

  // We are no longer able to schedule tasks.
  notifier.Schedule();
  tasks = TakePendingTasks();
  ASSERT_EQ(0u, tasks.size());

  // Verify after the scheduled time happens there is still no task.
  notifier.SetNow(notifier.Now() + delay);
  tasks = TakePendingTasks();
  ASSERT_EQ(0u, tasks.size());
}

TEST_F(DelayedUniqueNotifierTest, ShutdownPreventsSchedule) {
  base::TimeDelta delay = base::Microseconds(20);
  TestNotifier notifier(task_runner_.get(),
                        base::BindRepeating(&DelayedUniqueNotifierTest::Notify,
                                            base::Unretained(this)),
                        delay);

  EXPECT_EQ(0, NotificationCount());

  // Schedule for |delay| seconds from now.
  base::TimeTicks schedule_time = notifier.Now() + base::Microseconds(10);
  notifier.SetNow(schedule_time);

  // Shutdown the notifier.
  notifier.Shutdown();

  // Scheduling a task no longer does anything.
  notifier.Schedule();
  base::circular_deque<base::TestPendingTask> tasks = TakePendingTasks();
  ASSERT_EQ(0u, tasks.size());

  // Verify after the scheduled time happens there is still no task.
  notifier.SetNow(notifier.Now() + delay);
  tasks = TakePendingTasks();
  ASSERT_EQ(0u, tasks.size());
}

}  // namespace
}  // namespace cc
