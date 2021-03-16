// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/power_monitor/thermal_state_observer_mac.h"

#include <memory>
#include <queue>

#import <Foundation/Foundation.h>

#include "base/power_monitor/power_monitor.h"
#include "base/power_monitor/power_monitor_source.h"
#include "testing/gtest/include/gtest/gtest.h"

using DeviceThermalState = base::PowerThermalObserver::DeviceThermalState;

namespace base {

class ThermalStateObserverMacTest : public testing::Test {
 public:
  ThermalStateObserverMacTest() = default;
  ~ThermalStateObserverMacTest() override = default;

  void OnStateChange(DeviceThermalState state) { state_history_.push(state); }

  std::queue<DeviceThermalState> state_history_;
  std::unique_ptr<ThermalStateObserverMac> thermal_state_observer_;
};

// Verifies that a NSProcessInfoThermalStateDidChangeNotification produces the
// adequate OnStateChange() call.
TEST_F(ThermalStateObserverMacTest, StateChange) NS_AVAILABLE_MAC(10_10_3) {
  EXPECT_TRUE(state_history_.empty());

  // ThermalStateObserverMac sends the current thermal state on construction.
  thermal_state_observer_ =
      std::make_unique<ThermalStateObserverMac>(BindRepeating(
          &ThermalStateObserverMacTest::OnStateChange, Unretained(this)));
  EXPECT_EQ(state_history_.size(), 1u);
  state_history_.pop();

  thermal_state_observer_->state_for_testing_ = DeviceThermalState::kCritical;
  [NSNotificationCenter.defaultCenter
      postNotificationName:NSProcessInfoThermalStateDidChangeNotification
                    object:nil
                  userInfo:nil];
  EXPECT_EQ(state_history_.size(), 1u);
  EXPECT_EQ(state_history_.front(), DeviceThermalState::kCritical);
}

}  // namespace base
