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
#include "content/public/test/browser_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {

const char kDefaultLog1[] = "This is the first log from the browser";
const char kDefaultLog2[] = "This is the second log from the browser";

std::string GetKioskLogCollectionEnabledLog() {
  ash::KioskApp app = ash::kiosk::test::AutoLaunchKioskApp();
  return "Starting log collection for kiosk app: " + base::ToString(app.id());
}

content::WebContents* GetWebContents(Browser* browser) {
  if (!browser || !browser->tab_strip_model()) {
    return nullptr;
  }
  return browser->tab_strip_model()->GetActiveWebContents();
}

std::string ConsoleLogScript(std::string log) {
  return base::StringPrintf("console.log('%s');", log);
}

ash::KioskMixin::Config GetWebAppConfig() {
  return ash::KioskMixin::Config{
      /*name=*/"WebApp",
      ash::KioskMixin::AutoLaunchAccount{
          ash::KioskMixin::SimpleWebAppOption().account_id},
      {ash::KioskMixin::SimpleWebAppOption()}};
}

}  // namespace

class KioskAppLevelLogsTestBase : public MixinBasedInProcessBrowserTest {
 public:
  KioskAppLevelLogsTestBase(ash::KioskMixin::Config config, bool policy_enabled)
      : kiosk_{&mixin_host_,
               /*cached_configuration=*/config},
        policy_enabled_(policy_enabled) {}

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

  void SetUpOnMainThread() override {
    SetKioskAppLevelLogCollectionPolicy(IsPolicyEnabled());
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
  }

  void SetKioskAppLevelLogCollectionPolicy(bool enable) {
    ash::kiosk::test::CurrentProfile().GetPrefs()->SetBoolean(
        prefs::kKioskApplicationLogCollectionEnabled, enable);
  }

  void ExpectMessageInKioskLogs(const std::string message) {
    base::RunLoop().RunUntilIdle();
    std::string log_content;
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(base::ReadFileToString(log_file_path_, &log_content))
        << "Failed to read log file: " << log_file_path_.value();
    EXPECT_THAT(log_content, testing::ContainsRegex(message))
        << "Log file content:\n"
        << log_content;
  }

  void ExpectMessageNotInKioskLogs(const std::string message) {
    base::RunLoop().RunUntilIdle();
    std::string log_content;
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(base::ReadFileToString(log_file_path_, &log_content))
        << "Failed to read log file: " << log_file_path_.value();
    EXPECT_THAT(log_content, testing::Not(testing::ContainsRegex(message)))
        << "Log file content:\n"
        << log_content;
  }

  bool IsPolicyEnabled() { return policy_enabled_; }

 private:
  ash::KioskMixin kiosk_;
  bool policy_enabled_;
  base::ScopedTempDir temp_dir_;
  base::FilePath log_file_path_;
};

class KioskAppLevelLogsTest
    : public KioskAppLevelLogsTestBase,
      public testing::WithParamInterface<ash::KioskMixin::Config> {
 public:
  KioskAppLevelLogsTest()
      : KioskAppLevelLogsTestBase(GetParam(), /*policy_enabled=*/true) {}
};

IN_PROC_BROWSER_TEST_P(KioskAppLevelLogsTest, ShouldCollectLogs) {
  std::string log_message = base::StringPrintf(
      "kiosk_app_level_logs_manager.cc.*%s", GetKioskLogCollectionEnabledLog());

  ExpectMessageInKioskLogs(log_message);
}

// TODO(b:425645764) Add service worker logs collection test.

INSTANTIATE_TEST_SUITE_P(
    All,
    KioskAppLevelLogsTest,
    ::testing::ValuesIn(ash::KioskMixin::ConfigsToAutoLaunchEachAppType()));

class WebKioskAppLevelLogsPolicyDisabledTest
    : public KioskAppLevelLogsTestBase,
      public testing::WithParamInterface<ash::KioskMixin::Config> {
 public:
  WebKioskAppLevelLogsPolicyDisabledTest()
      : KioskAppLevelLogsTestBase(GetParam(), /*policy_enabled=*/false) {}
};

IN_PROC_BROWSER_TEST_P(WebKioskAppLevelLogsPolicyDisabledTest,
                       ShouldNotCollectLogs) {
  std::string log_message = base::StringPrintf(
      "kiosk_app_level_logs_manager.cc.*%s", GetKioskLogCollectionEnabledLog());

  ExpectMessageNotInKioskLogs(log_message);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    WebKioskAppLevelLogsPolicyDisabledTest,
    ::testing::ValuesIn(ash::KioskMixin::ConfigsToAutoLaunchEachAppType()));

class WebKioskAppLevelLogsTest : public KioskAppLevelLogsTestBase {
 public:
  WebKioskAppLevelLogsTest()
      : KioskAppLevelLogsTestBase(GetWebAppConfig(), /*policy_enabled=*/true) {}

  void SetUpOnMainThread() override {
    KioskAppLevelLogsTestBase::SetUpOnMainThread();
    ASSERT_TRUE(ash::kiosk::test::WaitKioskLaunched());
    SelectFirstBrowser();
    ExpectOnlyKioskAppOpen();
  }

 private:
  void ExpectOnlyKioskAppOpen() const {
    // The initial browser should exist in the web kiosk session.
    ASSERT_EQ(BrowserList::GetInstance()->size(), 1u);
    Browser* kiosk_browser = BrowserList::GetInstance()->get(0);
    ASSERT_EQ(kiosk_browser->tab_strip_model()->count(), 1);
    content::WebContents* contents =
        kiosk_browser->tab_strip_model()->GetActiveWebContents();
    ASSERT_TRUE(contents);
    if (contents->IsLoading()) {
      content::WaitForLoadStop(contents);
    }
  }
};

IN_PROC_BROWSER_TEST_F(WebKioskAppLevelLogsTest, ShouldCollectBrowserLogs) {
  EXPECT_TRUE(content::ExecJs(GetWebContents(browser()),
                              ConsoleLogScript(kDefaultLog1)));
  std::string log_message_1 =
      base::StringPrintf("kiosk_app_level_logs_saver.cc.*%s", kDefaultLog1);

  ExpectMessageInKioskLogs(log_message_1);

  EXPECT_TRUE(content::ExecJs(GetWebContents(browser()),
                              ConsoleLogScript(kDefaultLog2)));
  std::string log_message_2 =
      base::StringPrintf("kiosk_app_level_logs_saver.cc.*%s", kDefaultLog2);

  ExpectMessageInKioskLogs(log_message_2);
}

}  // namespace chromeos
