// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/instance_throttle/arc_switch_throttle_observer.h"

#include "ash/constants/ash_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/test/scoped_command_line.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

using ArcSwitchThrottleObserverTest = testing::Test;

TEST_F(ArcSwitchThrottleObserverTest, Default) {
  ArcSwitchThrottleObserver observer;
  int call_count = 0;
  observer.StartObserving(
      nullptr /* context */,
      base::BindRepeating([](int* counter) { (*counter)++; }, &call_count));

  EXPECT_EQ(0, call_count);
  EXPECT_FALSE(observer.active());
}

TEST_F(ArcSwitchThrottleObserverTest, Enforced) {
  base::test::ScopedCommandLine command_line;
  command_line.GetProcessCommandLine()->AppendSwitch(
      chromeos::switches::kDisableArcCpuRestriction);
  ArcSwitchThrottleObserver observer;
  int call_count = 0;
  observer.StartObserving(
      nullptr /* context */,
      base::BindRepeating([](int* counter) { (*counter)++; }, &call_count));

  EXPECT_EQ(1, call_count);
  EXPECT_TRUE(observer.active());
}

}  // namespace arc
