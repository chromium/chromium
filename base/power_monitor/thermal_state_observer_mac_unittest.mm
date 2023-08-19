// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/power_monitor/thermal_state_observer_mac.h"

#import <Foundation/Foundation.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <notify.h>

#include <memory>
#include <queue>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/power_monitor/power_monitor.h"
#include "base/power_monitor/power_monitor_source.h"
#include "base/synchronization/waitable_event.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using DeviceThermalState = base::PowerThermalObserver::DeviceThermalState;
using ::testing::MockFunction;
using ::testing::Mock;
using ::testing::Invoke;

namespace base {
void IgnoreStateChange(DeviceThermalState state) {}
void IgnoreSpeedLimitChange(int speed_limit) {}

// Verifies that a NSProcessInfoThermalStateDidChangeNotification produces the
// adequate OnStateChange() call.
TEST(ThermalStateObserverMacTest, StateChange) {
  MockFunction<void(DeviceThermalState)> function;
  // ThermalStateObserverMac sends the current thermal state on construction.
  EXPECT_CALL(function, Call);
  ThermalStateObserverMac observer(
      BindRepeating(&MockFunction<void(DeviceThermalState)>::Call,
                    Unretained(&function)),
      BindRepeating(IgnoreSpeedLimitChange), "ignored key");
  Mock::VerifyAndClearExpectations(&function);
  EXPECT_CALL(function, Call(DeviceThermalState::kCritical));
  observer.state_for_testing_ = DeviceThermalState::kCritical;
  [NSNotificationCenter.defaultCenter
      postNotificationName:NSProcessInfoThermalStateDidChangeNotification
                    object:nil
                  userInfo:nil];
}

TEST(ThermalStateObserverMacTest, SpeedChange) {
  MockFunction<void(int)> function;
  // ThermalStateObserverMac sends the current speed limit state on
  // construction.
  static constexpr const char* kTestNotificationKey =
      "ThermalStateObserverMacTest_SpeedChange";
  EXPECT_CALL(function, Call);
  ThermalStateObserverMac observer(
      BindRepeating(IgnoreStateChange),
      BindRepeating(&MockFunction<void(int)>::Call, Unretained(&function)),
      kTestNotificationKey);
  Mock::VerifyAndClearExpectations(&function);
  EXPECT_CALL(function, Call).WillOnce(Invoke([] {
    CFRunLoopStop(CFRunLoopGetCurrent());
  }));
  notify_post(kTestNotificationKey);
  CFRunLoopRun();
}
}  // namespace base
