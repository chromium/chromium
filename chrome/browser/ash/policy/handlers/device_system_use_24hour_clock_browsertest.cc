// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/system_tray_test_api.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#include "chrome/browser/ash/system/system_clock.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace em = ::enterprise_management;

class SystemUse24HourClockPolicyTest : public DevicePolicyCrosBrowserTest {
 public:
  SystemUse24HourClockPolicyTest() = default;

  SystemUse24HourClockPolicyTest(const SystemUse24HourClockPolicyTest&) =
      delete;
  SystemUse24HourClockPolicyTest& operator=(
      const SystemUse24HourClockPolicyTest&) = delete;

  // DevicePolicyCrosBrowserTest:
  void SetUpOnMainThread() override {
    DevicePolicyCrosBrowserTest::SetUpOnMainThread();
    tray_test_api_ = ash::SystemTrayTestApi::Create();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(ash::switches::kLoginManager);
    command_line->AppendSwitch(ash::switches::kForceLoginManagerInTests);
  }

  void TearDownOnMainThread() override {
    // If the login display is still showing, exit gracefully.
    if (ash::LoginDisplayHost::default_host()) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&chrome::AttemptExit));
      RunUntilBrowserProcessQuits();
    }
  }

 protected:
  void RefreshPolicyAndWaitDeviceSettingsUpdated() {
    base::RunLoop run_loop;
    base::CallbackListSubscription subscription =
        ash::CrosSettings::Get()->AddSettingsObserver(
            ash::kSystemUse24HourClock, run_loop.QuitWhenIdleClosure());

    RefreshDevicePolicy();
    run_loop.Run();
  }

  bool IsPrimarySystemTrayUse24Hour() {
    return tray_test_api_->Is24HourClock();
  }

  static bool SystemClockShouldUse24Hour() {
    return g_browser_process->platform_part()
        ->GetSystemClock()
        ->ShouldUse24HourClock();
  }

 private:
  std::unique_ptr<ash::SystemTrayTestApi> tray_test_api_;
};

IN_PROC_BROWSER_TEST_F(SystemUse24HourClockPolicyTest, CheckUnset) {
  bool system_use_24hour_clock;
  EXPECT_FALSE(ash::CrosSettings::Get()->GetBoolean(ash::kSystemUse24HourClock,
                                                    &system_use_24hour_clock));

  EXPECT_FALSE(SystemClockShouldUse24Hour());
  EXPECT_FALSE(IsPrimarySystemTrayUse24Hour());
}

IN_PROC_BROWSER_TEST_F(SystemUse24HourClockPolicyTest, CheckTrue) {
  bool system_use_24hour_clock = true;
  EXPECT_FALSE(ash::CrosSettings::Get()->GetBoolean(ash::kSystemUse24HourClock,
                                                    &system_use_24hour_clock));

  EXPECT_FALSE(SystemClockShouldUse24Hour());
  EXPECT_FALSE(IsPrimarySystemTrayUse24Hour());

  em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
  proto.mutable_use_24hour_clock()->set_use_24hour_clock(true);
  RefreshPolicyAndWaitDeviceSettingsUpdated();

  system_use_24hour_clock = false;
  EXPECT_TRUE(ash::CrosSettings::Get()->GetBoolean(ash::kSystemUse24HourClock,
                                                   &system_use_24hour_clock));
  EXPECT_TRUE(system_use_24hour_clock);
  EXPECT_TRUE(SystemClockShouldUse24Hour());
  EXPECT_TRUE(IsPrimarySystemTrayUse24Hour());
}

IN_PROC_BROWSER_TEST_F(SystemUse24HourClockPolicyTest, CheckFalse) {
  bool system_use_24hour_clock = true;
  EXPECT_FALSE(ash::CrosSettings::Get()->GetBoolean(ash::kSystemUse24HourClock,
                                                    &system_use_24hour_clock));

  EXPECT_FALSE(SystemClockShouldUse24Hour());
  EXPECT_FALSE(IsPrimarySystemTrayUse24Hour());

  em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
  proto.mutable_use_24hour_clock()->set_use_24hour_clock(false);
  RefreshPolicyAndWaitDeviceSettingsUpdated();

  system_use_24hour_clock = true;
  EXPECT_TRUE(ash::CrosSettings::Get()->GetBoolean(ash::kSystemUse24HourClock,
                                                   &system_use_24hour_clock));
  EXPECT_FALSE(system_use_24hour_clock);
  EXPECT_FALSE(SystemClockShouldUse24Hour());
  EXPECT_FALSE(IsPrimarySystemTrayUse24Hour());
}

}  // namespace policy
