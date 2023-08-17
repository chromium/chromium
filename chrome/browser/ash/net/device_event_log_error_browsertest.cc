// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/json/json_reader.h"
#include "base/values.h"
#include "build/buildflag.h"
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
    size_t error_count = device_event_log::GetCountByLevelForTesting(
        device_event_log::LogLevel::LOG_LEVEL_ERROR);
    if (error_count == 0u) {
      return error_count;
    }

// These tests check that there are no device event log errors output in the
// startup and login cases. However, since browser test startup/initialization
// does not match exactly the startup/initialization of production there can be
// specific cases where errors are output only in the test environment. These
// specific cases are filtered out below.
#if BUILDFLAG(IS_CHROMEOS)
    auto device_event_logs = base::JSONReader::Read(
        device_event_log::GetAsString(
            device_event_log::StringOrder::OLDEST_FIRST, /*format=*/"json",
            /*types=*/"", device_event_log::LOG_LEVEL_ERROR, /*max_events=*/0),
        base::JSON_ALLOW_TRAILING_COMMAS);
    if (!device_event_logs.has_value() || !device_event_logs->is_list()) {
      return error_count;
    }

    error_count = 0u;

    for (const auto& entry_json : device_event_logs->GetList()) {
      auto entry = base::JSONReader::ReadDict(entry_json.GetString(),
                                              base::JSON_ALLOW_TRAILING_COMMAS);
      if (!entry.has_value()) {
        error_count++;
        continue;
      }
      const std::string* file = entry->FindString("file");
      if (!file) {
        error_count++;
        continue;
      }

      // Bluetooth devices are not typically available immediately upon
      // construction, but they are for the testing stack. This results in
      // attempts to look up the nicknames for devices before device prefs are
      // available which will always fail. Filter these errors out.
      if (file->find("device_name_manager_impl.cc") == std::string::npos) {
        error_count++;
      }
    }
#endif
    return error_count;
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
