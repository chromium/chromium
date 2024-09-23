// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/timer/hi_res_timer_manager.h"

#include <memory>
#include <utility>

#include "base/base_switches.h"
#include "base/power_monitor/power_monitor.h"
#include "base/power_monitor/power_monitor_device_source.h"
#include "base/test/power_monitor_test.h"
#include "base/test/scoped_command_line.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

#if BUILDFLAG(IS_WIN)
TEST(HiResTimerManagerTest, ToggleOnOff) {
  test::TaskEnvironment task_environment;
  base::test::ScopedPowerMonitorTestSource power_monitor_source;

  HighResolutionTimerManager manager;

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
    power_monitor_source.GeneratePowerStateEvent(
        PowerStateObserver::BatteryPowerStatus::kBatteryPower);

    EXPECT_FALSE(manager.hi_res_clock_available());
    EXPECT_FALSE(base::Time::IsHighResolutionTimerInUse());

    // Back to on-AC power.
    power_monitor_source.GeneratePowerStateEvent(
        PowerStateObserver::BatteryPowerStatus::kExternalPower);
    EXPECT_TRUE(manager.hi_res_clock_available());
    EXPECT_TRUE(base::Time::IsHighResolutionTimerInUse());

    // De-activate the high resolution timer.
    base::Time::ActivateHighResolutionTimer(false);
  }
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
#endif  // BUILDFLAG(IS_WIN)

}  // namespace base
