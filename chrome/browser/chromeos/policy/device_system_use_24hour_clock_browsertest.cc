// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/system_tray_test_api.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/policy/device_policy_cros_browser_test.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/system/system_clock.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chromeos/constants/chromeos_switches.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

namespace chromeos {

class SystemUse24HourClockPolicyTest
    : public policy::DevicePolicyCrosBrowserTest {
 public:
  SystemUse24HourClockPolicyTest() = default;

  // policy::DevicePolicyCrosBrowserTest:
  void SetUpOnMainThread() override {
    policy::DevicePolicyCrosBrowserTest::SetUpOnMainThread();
    tray_test_api_ = ash::SystemTrayTestApi::Create();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kLoginManager);
    command_line->AppendSwitch(chromeos::switches::kForceLoginManagerInTests);
  }

  void TearDownOnMainThread() override {
    // If the login display is still showing, exit gracefully.
    if (LoginDisplayHost::default_host()) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(&chrome::AttemptExit));
      RunUntilBrowserProcessQuits();
    }
  }

 protected:
  void RefreshPolicyAndWaitDeviceSettingsUpdated() {
    base::RunLoop run_loop;
    std::unique_ptr<CrosSettings::ObserverSubscription> observer =
        CrosSettings::Get()->AddSettingsObserver(
            kSystemUse24HourClock, run_loop.QuitWhenIdleClosure());

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

  DISALLOW_COPY_AND_ASSIGN(SystemUse24HourClockPolicyTest);
};

IN_PROC_BROWSER_TEST_F(SystemUse24HourClockPolicyTest, CheckUnset) {
  bool system_use_24hour_clock;
  EXPECT_FALSE(CrosSettings::Get()->GetBoolean(kSystemUse24HourClock,
                                               &system_use_24hour_clock));

  EXPECT_FALSE(SystemClockShouldUse24Hour());
  EXPECT_FALSE(IsPrimarySystemTrayUse24Hour());
}

IN_PROC_BROWSER_TEST_F(SystemUse24HourClockPolicyTest, CheckTrue) {
  bool system_use_24hour_clock = true;
  EXPECT_FALSE(CrosSettings::Get()->GetBoolean(kSystemUse24HourClock,
                                               &system_use_24hour_clock));

  EXPECT_FALSE(SystemClockShouldUse24Hour());
  EXPECT_FALSE(IsPrimarySystemTrayUse24Hour());

  em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
  proto.mutable_use_24hour_clock()->set_use_24hour_clock(true);
  RefreshPolicyAndWaitDeviceSettingsUpdated();

  system_use_24hour_clock = false;
  EXPECT_TRUE(CrosSettings::Get()->GetBoolean(kSystemUse24HourClock,
                                              &system_use_24hour_clock));
  EXPECT_TRUE(system_use_24hour_clock);
  EXPECT_TRUE(SystemClockShouldUse24Hour());
  EXPECT_TRUE(IsPrimarySystemTrayUse24Hour());
}

IN_PROC_BROWSER_TEST_F(SystemUse24HourClockPolicyTest, CheckFalse) {
  bool system_use_24hour_clock = true;
  EXPECT_FALSE(CrosSettings::Get()->GetBoolean(kSystemUse24HourClock,
                                               &system_use_24hour_clock));

  EXPECT_FALSE(SystemClockShouldUse24Hour());
  EXPECT_FALSE(IsPrimarySystemTrayUse24Hour());

  em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
  proto.mutable_use_24hour_clock()->set_use_24hour_clock(false);
  RefreshPolicyAndWaitDeviceSettingsUpdated();

  system_use_24hour_clock = true;
  EXPECT_TRUE(CrosSettings::Get()->GetBoolean(kSystemUse24HourClock,
                                              &system_use_24hour_clock));
  EXPECT_FALSE(system_use_24hour_clock);
  EXPECT_FALSE(SystemClockShouldUse24Hour());
  EXPECT_FALSE(IsPrimarySystemTrayUse24Hour());
}

}  // namespace chromeos
