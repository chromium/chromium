// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/base/unique_notifier.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

class UniqueNotifierTest : public testing::Test {
 public:
  UniqueNotifierTest() : notification_count_(0) {}

  void SetUp() override { ResetNotificationCount(); }

  void Notify() { ++notification_count_; }

  int NotificationCount() const { return notification_count_; }

  void ResetNotificationCount() { notification_count_ = 0; }

 protected:
  int notification_count_;
};

// Need to guarantee that Schedule and Notify happen in the same thread.
// Multiple schedules may result in multiple runs when notify task is posted to
// a different thread. So we use thread checker to avoid this.
// Example which may result in multiple runs:
//   base::Thread notifier_thread("NotifierThread");
//   notifier_thread.Start();
//   UniqueNotifier notifier(
//       notifier_thread.task_runner().get(),
//       base::BindRepeating(&UniqueNotifierTest::Notify,
//       base::Unretained(this)));
//   EXPECT_EQ(0, NotificationCount());
//   for (int i = 0; i < 50000; ++i)
//     notifier.Schedule();
//   base::RunLoop().RunUntilIdle();

//   notifier_thread.Stop();
//   EXPECT_LE(1, NotificationCount());
// 50000 can be any number bigger than 1. The bigger the easier to more runs.
TEST_F(UniqueNotifierTest, Schedule) {
  {
    UniqueNotifier notifier(
        base::SingleThreadTaskRunner::GetCurrentDefault().get(),
        base::BindRepeating(&UniqueNotifierTest::Notify,
                            base::Unretained(this)));

    EXPECT_EQ(0, NotificationCount());

    // Basic schedule should result in a run.
    notifier.Schedule();

    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(1, NotificationCount());

    // UniqueNotifier can only runs in the main thread, and multiple schedules
    // should result in one run.
    for (int i = 0; i < 5; ++i)
      notifier.Schedule();

    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(2, NotificationCount());

    // Schedule and cancel.
    notifier.Schedule();
    notifier.Cancel();

    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(2, NotificationCount());

    notifier.Schedule();
    notifier.Cancel();
    notifier.Schedule();

    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(3, NotificationCount());

    notifier.Schedule();
  }

  // Notifier went out of scope, so we don't expect to get a notification.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(3, NotificationCount());
}

}  // namespace
}  // namespace cc
