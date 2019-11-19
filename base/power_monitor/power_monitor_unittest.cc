// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/power_monitor/power_monitor.h"

#include "base/macros.h"
#include "base/test/power_monitor_test_base.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

class PowerMonitorTest : public testing::Test {
 protected:
  PowerMonitorTest() {
    power_monitor_source_ = new PowerMonitorTestSource();
    PowerMonitor::Initialize(
        std::unique_ptr<PowerMonitorSource>(power_monitor_source_));
  }
  ~PowerMonitorTest() override { PowerMonitor::ShutdownForTesting(); }

  PowerMonitorTestSource* source() { return power_monitor_source_; }

 private:
  test::TaskEnvironment task_environment_;
  PowerMonitorTestSource* power_monitor_source_;

  DISALLOW_COPY_AND_ASSIGN(PowerMonitorTest);
};

// PowerMonitorSource is tightly coupled with the PowerMonitor, so this test
// covers both classes.
TEST_F(PowerMonitorTest, PowerNotifications) {
  const int kObservers = 5;

  PowerMonitorTestObserver observers[kObservers];
  for (auto& index : observers)
    EXPECT_TRUE(PowerMonitor::AddObserver(&index));

  // Sending resume when not suspended should have no effect.
  source()->GenerateResumeEvent();
  EXPECT_EQ(observers[0].resumes(), 0);

  // Pretend we suspended.
  source()->GenerateSuspendEvent();
  // Ensure all observers were notified of the event
  for (const auto& index : observers)
    EXPECT_EQ(index.suspends(), 1);

  // Send a second suspend notification.  This should be suppressed.
  source()->GenerateSuspendEvent();
  EXPECT_EQ(observers[0].suspends(), 1);

  // Pretend we were awakened.
  source()->GenerateResumeEvent();
  EXPECT_EQ(observers[0].resumes(), 1);

  // Send a duplicate resume notification.  This should be suppressed.
  source()->GenerateResumeEvent();
  EXPECT_EQ(observers[0].resumes(), 1);

  // Pretend the device has gone on battery power
  source()->GeneratePowerStateEvent(true);
  EXPECT_EQ(observers[0].power_state_changes(), 1);
  EXPECT_EQ(observers[0].last_power_state(), true);

  // Repeated indications the device is on battery power should be suppressed.
  source()->GeneratePowerStateEvent(true);
  EXPECT_EQ(observers[0].power_state_changes(), 1);

  // Pretend the device has gone off battery power
  source()->GeneratePowerStateEvent(false);
  EXPECT_EQ(observers[0].power_state_changes(), 2);
  EXPECT_EQ(observers[0].last_power_state(), false);

  // Repeated indications the device is off battery power should be suppressed.
  source()->GeneratePowerStateEvent(false);
  EXPECT_EQ(observers[0].power_state_changes(), 2);

  for (auto& index : observers)
    PowerMonitor::RemoveObserver(&index);
}

}  // namespace base
