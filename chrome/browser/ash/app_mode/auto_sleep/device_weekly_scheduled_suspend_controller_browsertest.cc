// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/auto_sleep/device_weekly_scheduled_suspend_controller.h"

#include <cstddef>

#include "chrome/browser/ash/app_mode/auto_sleep/device_weekly_scheduled_suspend_test_policy_builder.h"
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_manager.h"
#include "chrome/browser/ash/login/app_mode/test/web_kiosk_base_test.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

// TODO(b/329616257): Extend browser test coverage.
class DeviceWeeklyScheduledSuspendControllerTest : public WebKioskBaseTest {
 protected:
  // WebKioskBaseTest:
  void SetUp() override {
    WebKioskBaseTest::SetUp();
    chromeos::PowerManagerClient::InitializeFake();
  }

  void TearDown() override {
    chromeos::PowerManagerClient::Shutdown();
    WebKioskBaseTest::TearDown();
  }
};

IN_PROC_BROWSER_TEST_F(DeviceWeeklyScheduledSuspendControllerTest,
                       SuspendControllerExistOnKioskStartUp) {
  {
    InitializeRegularOnlineKiosk();
    ASSERT_TRUE(WebKioskAppManager::Get()->kiosk_system_session());

    ASSERT_TRUE(WebKioskAppManager::Get()
                    ->kiosk_system_session()
                    ->device_weekly_scheduled_suspend_controller_for_testing());
  }
}

}  // namespace ash
