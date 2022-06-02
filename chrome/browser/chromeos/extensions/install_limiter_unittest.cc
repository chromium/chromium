// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/install_limiter.h"

#include "ash/components/tpm/stub_install_attributes.h"
#include "chrome/browser/ash/login/demo_mode/demo_mode_test_helper.h"
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/common/constants.h"
#include "testing/gtest/include/gtest/gtest.h"

using extensions::InstallLimiter;

namespace {

constexpr char kRandomExtensionId[] = "abacabadabacabaeabacabadabacabaf";
constexpr int kLargeExtensionSize = 2000000;
constexpr int kSmallExtensionSize = 200000;

}  // namespace

class InstallLimiterTest
    : public testing::TestWithParam<ash::DemoSession::DemoModeConfig> {
 public:
  InstallLimiterTest()
      : scoped_user_manager_(std::make_unique<ash::FakeChromeUserManager>()) {}

  InstallLimiterTest(const InstallLimiterTest&) = delete;
  InstallLimiterTest& operator=(const InstallLimiterTest&) = delete;

  ~InstallLimiterTest() override = default;

 private:
  content::BrowserTaskEnvironment task_environment_;
  ash::ScopedStubInstallAttributes test_install_attributes_;
  user_manager::ScopedUserManager scoped_user_manager_;
};

TEST_P(InstallLimiterTest, ShouldDeferInstall) {
  const std::vector<std::string> screensaver_ids = {
      extension_misc::kScreensaverAppId, extension_misc::kScreensaverAtlasAppId,
      extension_misc::kScreensaverKraneZdksAppId};

  ash::DemoModeTestHelper demo_mode_test_helper;
  if (GetParam() != ash::DemoSession::DemoModeConfig::kNone)
    demo_mode_test_helper.InitializeSession(GetParam());

  // In demo mode, all apps larger than 1MB except for the screensaver
  // should be deferred.
  for (const std::string& id : screensaver_ids) {
    bool expected_defer_install =
        GetParam() == ash::DemoSession::DemoModeConfig::kNone ||
        id != ash::DemoSession::GetScreensaverAppId();
    EXPECT_EQ(expected_defer_install,
              InstallLimiter::ShouldDeferInstall(kLargeExtensionSize, id));
  }
  EXPECT_TRUE(InstallLimiter::ShouldDeferInstall(kLargeExtensionSize,
                                                 kRandomExtensionId));
  EXPECT_FALSE(InstallLimiter::ShouldDeferInstall(kSmallExtensionSize,
                                                  kRandomExtensionId));
}

INSTANTIATE_TEST_SUITE_P(
    DemoModeConfig,
    InstallLimiterTest,
    ::testing::Values(ash::DemoSession::DemoModeConfig::kNone,
                      ash::DemoSession::DemoModeConfig::kOnline));
