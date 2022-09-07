// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/sequence_manager/thread_controller_power_monitor.h"

#include "base/power_monitor/power_monitor.h"
#include "base/power_monitor/power_monitor_source.h"
#include "base/test/power_monitor_test.h"
#include "base/test/task_environment.h"

#include "base/test/mock_callback.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace sequence_manager {
namespace internal {

class ThreadControllerPowerMonitorTest : public testing::Test {
 public:
  void SetUp() override {
    thread_controller_power_monitor_ =
        std::make_unique<ThreadControllerPowerMonitor>();
    internal::ThreadControllerPowerMonitor::OverrideUsePowerMonitorForTesting(
        true);
  }

  void TearDown() override {
    thread_controller_power_monitor_.reset();
    internal::ThreadControllerPowerMonitor::ResetForTesting();
  }

 protected:
  test::SingleThreadTaskEnvironment task_environment_;
  test::ScopedPowerMonitorTestSource power_monitor_source_;
  std::unique_ptr<ThreadControllerPowerMonitor>
      thread_controller_power_monitor_;
};

TEST_F(ThreadControllerPowerMonitorTest, IsProcessInPowerSuspendState) {
  EXPECT_FALSE(
      thread_controller_power_monitor_->IsProcessInPowerSuspendState());

  // Before the monitor is bound to the thread, the notifications are not
  // received.
  power_monitor_source_.GenerateSuspendEvent();
  EXPECT_FALSE(
      thread_controller_power_monitor_->IsProcessInPowerSuspendState());
  power_monitor_source_.GenerateResumeEvent();
  EXPECT_FALSE(
      thread_controller_power_monitor_->IsProcessInPowerSuspendState());

  thread_controller_power_monitor_->BindToCurrentThread();

  // Ensures notifications are processed.
  power_monitor_source_.GenerateSuspendEvent();
  EXPECT_TRUE(thread_controller_power_monitor_->IsProcessInPowerSuspendState());
  power_monitor_source_.GenerateResumeEvent();
  EXPECT_FALSE(
      thread_controller_power_monitor_->IsProcessInPowerSuspendState());
}

}  // namespace internal
}  // namespace sequence_manager
}  // namespace base
