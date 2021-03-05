// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/login_screen_extensions_lifetime_manager.h"

#include <string>

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/path_service.h"
#include "chrome/browser/ash/login/lock/screen_locker_tester.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/policy/extension_force_install_mixin.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_test.h"
#include "extensions/test/test_background_page_ready_observer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {

constexpr char kAllowlistedAppId[] = "bjaiihebfngildkcjkjckolinodhliff";
constexpr char kAllowlistedAppCrxPath[] =
    "extensions/signin_screen_manual_test_app/app_signed_by_webstore.crx";

// Returns the profile into which login-screen extensions are force-installed.
Profile* GetOriginalSigninProfile() {
  return chromeos::ProfileHelper::GetSigninProfile()->GetOriginalProfile();
}

}  // namespace

class LoginScreenExtensionsLifetimeManagerTest
    : public MixinBasedInProcessBrowserTest {
 protected:
  LoginScreenExtensionsLifetimeManagerTest() {
    // Don't shut down when no browser is open, since it breaks the test and
    // since it's not the real Chrome OS behavior.
    set_exit_when_last_browser_closes(false);
  }

  LoginScreenExtensionsLifetimeManagerTest(
      const LoginScreenExtensionsLifetimeManagerTest&) = delete;
  LoginScreenExtensionsLifetimeManagerTest& operator=(
      const LoginScreenExtensionsLifetimeManagerTest&) = delete;
  ~LoginScreenExtensionsLifetimeManagerTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Skip showing post-login screens, as advancing through them isn't faked in
    // the test.
    command_line->AppendSwitch(switches::kOobeSkipPostLogin);

    MixinBasedInProcessBrowserTest::SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
    extension_force_install_mixin_.InitWithDeviceStateMixin(
        GetOriginalSigninProfile(), &device_state_mixin_);
  }

  void LogIn() {
    login_manager_mixin_.LoginAsNewRegularUser();
    login_manager_mixin_.WaitForActiveSession();
  }

  void LockSession() { ScreenLockerTester().Lock(); }

  void UnlockSession() {
    const std::string kPassword = "pass";
    const AccountId account_id =
        user_manager::UserManager::Get()->GetPrimaryUser()->GetAccountId();
    ScreenLockerTester screen_locker_tester;
    screen_locker_tester.SetUnlockPassword(account_id, kPassword);
    screen_locker_tester.UnlockWithPassword(account_id, kPassword);
    screen_locker_tester.WaitForUnlock();
  }

  ExtensionForceInstallMixin* extension_force_install_mixin() {
    return &extension_force_install_mixin_;
  }

  bool IsExtensionInstalled(const std::string& extension_id) const {
    return extension_force_install_mixin_.GetInstalledExtension(extension_id) !=
           nullptr;
  }

  bool IsExtensionEnabled(const std::string& extension_id) const {
    return extension_force_install_mixin_.GetEnabledExtension(extension_id) !=
           nullptr;
  }

  bool IsExtensionBackgroundPageReady(const std::string& extension_id) const {
    return extension_force_install_mixin_.IsExtensionBackgroundPageReady(
        extension_id);
  }

 private:
  DeviceStateMixin device_state_mixin_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};
  LoginManagerMixin login_manager_mixin_{&mixin_host_};
  ExtensionForceInstallMixin extension_force_install_mixin_{&mixin_host_};
};

// Tests that an extension force-installed on the login/lock screen gets
// disabled during an active user session.
IN_PROC_BROWSER_TEST_F(LoginScreenExtensionsLifetimeManagerTest, Basic) {
  // Force-install the app while on the login screen. The app gets loaded.
  EXPECT_TRUE(extension_force_install_mixin()->ForceInstallFromCrx(
      base::PathService::CheckedGet(chrome::DIR_TEST_DATA)
          .AppendASCII(kAllowlistedAppCrxPath),
      ExtensionForceInstallMixin::WaitMode::kBackgroundPageReady));
  ASSERT_TRUE(IsExtensionEnabled(kAllowlistedAppId));
  EXPECT_TRUE(IsExtensionBackgroundPageReady(kAllowlistedAppId));

  // The user logs in. The app gets disabled, although still installed.
  LogIn();
  EXPECT_TRUE(IsExtensionInstalled(kAllowlistedAppId));
  EXPECT_FALSE(IsExtensionEnabled(kAllowlistedAppId));

  // The user locks the session. The app gets enabled and the background page
  // is loaded again.
  extensions::ExtensionBackgroundPageReadyObserver page_observer(
      GetOriginalSigninProfile(), kAllowlistedAppId);
  LockSession();
  page_observer.Wait();
  ASSERT_TRUE(IsExtensionEnabled(kAllowlistedAppId));

  // The user unlocks the session. The app gets disabled again.
  UnlockSession();
  EXPECT_TRUE(IsExtensionInstalled(kAllowlistedAppId));
  EXPECT_FALSE(IsExtensionEnabled(kAllowlistedAppId));
}

// Tests that an extension force-installed into the login/lock screen when
// they're not active doesn't get launched.
IN_PROC_BROWSER_TEST_F(LoginScreenExtensionsLifetimeManagerTest,
                       InstalledDuringSession) {
  // Force-install the app during an active user session. The app gets
  // installed, but is immediately disabled.
  LogIn();
  EXPECT_TRUE(extension_force_install_mixin()->ForceInstallFromCrx(
      base::PathService::CheckedGet(chrome::DIR_TEST_DATA)
          .AppendASCII(kAllowlistedAppCrxPath),
      ExtensionForceInstallMixin::WaitMode::kLoad));
  EXPECT_TRUE(IsExtensionInstalled(kAllowlistedAppId));
  EXPECT_FALSE(IsExtensionEnabled(kAllowlistedAppId));

  // The user locks the session. The app gets enabled and the background page is
  // loaded again.
  extensions::ExtensionBackgroundPageReadyObserver page_observer(
      GetOriginalSigninProfile(), kAllowlistedAppId);
  LockSession();
  page_observer.Wait();
  ASSERT_TRUE(IsExtensionEnabled(kAllowlistedAppId));
  EXPECT_TRUE(IsExtensionBackgroundPageReady(kAllowlistedAppId));
}

}  // namespace chromeos
