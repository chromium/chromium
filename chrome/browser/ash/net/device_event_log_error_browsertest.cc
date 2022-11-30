// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "chrome/browser/ash/login/login_manager_test.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "components/account_id/account_id.h"
#include "components/device_event_log/device_event_log.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

// Browser test using device_event_log to ensure that no device related error
// events are generated during startup and after login.

namespace ash {

class DeviceEventLogErrorBrowserTest : public LoginManagerTest {
 public:
  DeviceEventLogErrorBrowserTest() {
    set_should_launch_browser(true);
    login_mixin_.AppendRegularUsers(1);
  }
  DeviceEventLogErrorBrowserTest(const DeviceEventLogErrorBrowserTest&) =
      delete;
  DeviceEventLogErrorBrowserTest& operator=(
      const DeviceEventLogErrorBrowserTest&) = delete;
  ~DeviceEventLogErrorBrowserTest() override = default;

  int GetErrors() {
    return device_event_log::GetCountByLevelForTesting(
        device_event_log::LogLevel::LOG_LEVEL_ERROR);
  }

 protected:
  LoginManagerMixin login_mixin_{&mixin_host_};
};

IN_PROC_BROWSER_TEST_F(DeviceEventLogErrorBrowserTest, Startup) {
  EXPECT_EQ(GetErrors(), 0);
}

IN_PROC_BROWSER_TEST_F(DeviceEventLogErrorBrowserTest, LoginUser) {
  LoginUser(login_mixin_.users()[0].account_id);
  EXPECT_EQ(GetErrors(), 0);
}

}  // namespace ash
