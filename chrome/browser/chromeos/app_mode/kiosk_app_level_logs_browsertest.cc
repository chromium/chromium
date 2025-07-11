// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <string>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/to_string.h"
#include "chrome/browser/ash/app_mode/test/kiosk_mixin.h"
#include "chrome/browser/ash/app_mode/test/kiosk_test_utils.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_level_logs_manager.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "components/prefs/pref_service.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {

std::string GetKioskLogCollectionEnabledLog() {
  ash::KioskApp app = ash::kiosk::test::AutoLaunchKioskApp();
  return "Starting log collection for kiosk app: " + base::ToString(app.id());
}

}  // namespace

class KioskAppLevelLogsTest
    : public MixinBasedInProcessBrowserTest,
      public testing::WithParamInterface<ash::KioskMixin::Config> {
 public:
  KioskAppLevelLogsTest() = default;
  ~KioskAppLevelLogsTest() = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    log_file_path_ = temp_dir_.GetPath().AppendASCII("test.log");
    MixinBasedInProcessBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(::switches::kEnableLogging);
    command_line->AppendSwitchPath(::switches::kLogFile, log_file_path_);
    MixinBasedInProcessBrowserTest::SetUpCommandLine(command_line);
  }

  void SetKioskAppLevelLogCollectionPolicy(bool enable) {
    ash::kiosk::test::CurrentProfile().GetPrefs()->SetBoolean(
        prefs::kKioskApplicationLogCollectionEnabled, enable);
  }

  void ExpectMessageInLogs(const std::string message) {
    base::RunLoop().RunUntilIdle();
    std::string log_content;
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(base::ReadFileToString(log_file_path_, &log_content))
        << "Failed to read log file: " << log_file_path_.value();
    EXPECT_THAT(log_content, testing::HasSubstr(message))
        << "Log file content:\n"
        << log_content;
  }

  void ExpectMessageNotInLogs(const std::string message) {
    base::RunLoop().RunUntilIdle();
    std::string log_content;
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(base::ReadFileToString(log_file_path_, &log_content))
        << "Failed to read log file: " << log_file_path_.value();
    EXPECT_THAT(log_content, testing::Not(testing::HasSubstr(message)))
        << "Log file content:\n"
        << log_content;
  }

 private:
  ash::KioskMixin kiosk_{&mixin_host_,
                         /*cached_configuration=*/GetParam()};
  base::ScopedTempDir temp_dir_;
  base::FilePath log_file_path_;
};

IN_PROC_BROWSER_TEST_P(KioskAppLevelLogsTest, ShouldLogIfPolicyIsEnabled) {
  SetKioskAppLevelLogCollectionPolicy(/*enable=*/true);

  ExpectMessageInLogs(GetKioskLogCollectionEnabledLog());
}

IN_PROC_BROWSER_TEST_P(KioskAppLevelLogsTest, ShouldNotLogIfPolicyIsDisabled) {
  SetKioskAppLevelLogCollectionPolicy(/*enable=*/false);

  ExpectMessageNotInLogs(GetKioskLogCollectionEnabledLog());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    KioskAppLevelLogsTest,
    testing::ValuesIn(ash::KioskMixin::ConfigsToAutoLaunchEachAppType()),
    ash::KioskMixin::ConfigName);

}  // namespace chromeos
