// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/timer/hi_res_timer_manager.h"

#include <memory>
#include <utility>

#include "base/base_switches.h"
#include "base/power_monitor/power_monitor.h"
#include "base/power_monitor/power_monitor_device_source.h"
#include "base/test/scoped_command_line.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

#if defined(OS_WIN)
TEST(HiResTimerManagerTest, ToggleOnOff) {
  // The power monitor creates Window to receive power notifications from
  // Windows, which makes this test flaky if you run while the machine
  // goes in or out of AC power.
  test::TaskEnvironment task_environment(
      test::TaskEnvironment::MainThreadType::UI);
  base::PowerMonitor::Initialize(
      std::make_unique<base::PowerMonitorDeviceSource>());

  {
    HighResolutionTimerManager manager;
    // Simulate a on-AC power event to get to a known initial state.
    manager.OnPowerStateChange(false);

    // Loop a few times to test power toggling.
    for (int times = 0; times != 3; ++times) {
      // The manager has the high resolution clock enabled now.
      EXPECT_TRUE(manager.hi_res_clock_available());
      // But the Time class has it off, because it hasn't been activated.
      EXPECT_FALSE(base::Time::IsHighResolutionTimerInUse());

      // Activate the high resolution timer.
      base::Time::ActivateHighResolutionTimer(true);
      EXPECT_TRUE(base::Time::IsHighResolutionTimerInUse());

      // Simulate a on-battery power event.
      manager.OnPowerStateChange(true);
      EXPECT_FALSE(manager.hi_res_clock_available());
      EXPECT_FALSE(base::Time::IsHighResolutionTimerInUse());

      // Back to on-AC power.
      manager.OnPowerStateChange(false);
      EXPECT_TRUE(manager.hi_res_clock_available());
      EXPECT_TRUE(base::Time::IsHighResolutionTimerInUse());

      // De-activate the high resolution timer.
      base::Time::ActivateHighResolutionTimer(false);
    }
  }
  base::PowerMonitor::ShutdownForTesting();
}

TEST(HiResTimerManagerTest, DisableFromCommandLine) {
  base::test::ScopedCommandLine command_line;
  command_line.GetProcessCommandLine()->AppendSwitch(
      switches::kDisableHighResTimer);

  // Reset to known initial state. Test suite implementation
  // enables the high resolution timer by default.
  Time::EnableHighResolutionTimer(false);

  HighResolutionTimerManager manager;

  // The high resolution clock is disabled via the command line flag.
  EXPECT_FALSE(manager.hi_res_clock_available());

  // Time class has it off as well, because it hasn't been activated.
  EXPECT_FALSE(base::Time::IsHighResolutionTimerInUse());

  // Try to activate the high resolution timer.
  base::Time::ActivateHighResolutionTimer(true);
  EXPECT_FALSE(base::Time::IsHighResolutionTimerInUse());

  // De-activate the high resolution timer.
  base::Time::ActivateHighResolutionTimer(false);

  // Re-enable the high-resolution timer for testing.
  Time::EnableHighResolutionTimer(true);
}
#endif  // defined(OS_WIN)

}  // namespace base
