// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/power_monitor/thermal_state_observer_mac.h"

#import <Foundation/Foundation.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <notify.h>

#include <tuple>

#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/power_monitor/power_monitor.h"
#include "base/power_monitor/power_monitor_source.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "testing/gtest/include/gtest/gtest.h"

using DeviceThermalState = base::PowerThermalObserver::DeviceThermalState;

namespace base {

class ThermalStateObserverMacTest : public testing::Test {
 public:
  ThermalStateObserverMacTest() = default;
  ~ThermalStateObserverMacTest() override = default;

 protected:
  // MainThreadType::UI is required to pump dispatch_get_main_queue(), which is
  // used by notify_register_dispatch in the production code.
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};
};

// Verifies that a NSProcessInfoThermalStateDidChangeNotification produces the
// adequate OnStateChange() call.
TEST_F(ThermalStateObserverMacTest, StateChange) {
  base::test::TestFuture<DeviceThermalState> state_future;

  // The observer fires the callback with the current state on construction.
  ThermalStateObserverMac observer(state_future.GetRepeatingCallback(),
                                   base::DoNothing(), "ignored key");

  // Consume the initial value (likely kUnknown or the actual system state).
  std::ignore = state_future.Take();

  // Set the specific state we want to test.
  observer.state_for_testing_ = DeviceThermalState::kCritical;

  // Trigger the system notification.
  [NSNotificationCenter.defaultCenter
      postNotificationName:NSProcessInfoThermalStateDidChangeNotification
                    object:nil
                  userInfo:nil];

  EXPECT_EQ(state_future.Take(), DeviceThermalState::kCritical);
}

TEST_F(ThermalStateObserverMacTest, SpeedChange) {
  base::test::TestFuture<int> speed_future;
  static constexpr const char* kTestNotificationKey =
      "ThermalStateObserverMacTest_SpeedChange";

  ThermalStateObserverMac observer(base::DoNothing(),
                                   speed_future.GetRepeatingCallback(),
                                   kTestNotificationKey);

  // The observer posts a background task on construction to read the initial
  // speed limit. Wait for this to complete to ensure the observer is fully
  // initialized before triggering the notification.
  EXPECT_NE(speed_future.Take(), -1);

  // Trigger the system notification. Verifying the status ensures the OS
  // notification system is functioning correctly within the test environment.
  uint32_t status = notify_post(kTestNotificationKey);
  ASSERT_EQ(status, NOTIFY_STATUS_OK);

  // Wait for the notification to be processed.
  EXPECT_NE(speed_future.Take(), -1);
}

}  // namespace base
