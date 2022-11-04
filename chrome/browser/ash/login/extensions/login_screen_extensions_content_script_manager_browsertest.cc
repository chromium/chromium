// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/extensions/login_screen_extensions_content_script_manager.h"

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
#include "content/public/test/browser_test.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/common/switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

// Extensions with a correct content script.
constexpr char kGoodExtensionId[] = "jpbinmmnncmfffhfpeonjmcgnapcholg";
constexpr char kGoodExtensionPath[] =
    "extensions/signin_screen_content_script_extension/good_extension/"
    "extension/";
constexpr char kGoodExtensionPemPath[] =
    "extensions/signin_screen_content_script_extension/good_extension/"
    "extension.pem";

// Extensions with an incorrect content script.
constexpr char kBadExtensionId[] = "edehlkcoilmenhomhdakcpolbbimkhlg";
constexpr char kBadExtensionPath[] =
    "extensions/signin_screen_content_script_extension/bad_extension/"
    "extension/";
constexpr char kBadExtensionPemPath[] =
    "extensions/signin_screen_content_script_extension/bad_extension/"
    "extension.pem";

// Returns the profile into which login-screen extensions are force-installed.
Profile* GetOriginalSigninProfile() {
  return ProfileHelper::GetSigninProfile()->GetOriginalProfile();
}

}  // namespace

// Tests that the LoginScreenExtensionsContentScriptManager allows the use of
// login screen extensions with content scripts if they match the allowlisted
// URL. The test cases are split in two classes because we can only set one
// extension ID in the command line flag kAllowlistedExtensionID.
class LoginScreenExtensionsGoodContentScriptTest
    : public MixinBasedInProcessBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Skip showing post-login screens, as advancing through them isn't faked in
    // the test.
    command_line->AppendSwitch(switches::kOobeSkipPostLogin);
    command_line->AppendSwitchASCII(
        extensions::switches::kAllowlistedExtensionID, kGoodExtensionId);

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

 private:
  DeviceStateMixin device_state_mixin_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};
  LoginManagerMixin login_manager_mixin_{&mixin_host_};
  ExtensionForceInstallMixin extension_force_install_mixin_{&mixin_host_};
};

// Tests that a force-installed extension with an allowed content script is
// enabled on the login/lock screen.
IN_PROC_BROWSER_TEST_F(LoginScreenExtensionsGoodContentScriptTest,
                       GoodExtensionEnabled) {
  extensions::TestExtensionRegistryObserver login_screen_observer(
      extensions::ExtensionRegistry::Get(GetOriginalSigninProfile()),
      kGoodExtensionId);
  EXPECT_TRUE(extension_force_install_mixin()->ForceInstallFromSourceDir(
      base::PathService::CheckedGet(chrome::DIR_TEST_DATA)
          .AppendASCII(kGoodExtensionPath),
      base::PathService::CheckedGet(chrome::DIR_TEST_DATA)
          .AppendASCII(kGoodExtensionPemPath),
      ExtensionForceInstallMixin::WaitMode::kLoad));
  login_screen_observer.WaitForExtensionReady();
  EXPECT_TRUE(IsExtensionInstalled(kGoodExtensionId));
  ASSERT_TRUE(IsExtensionEnabled(kGoodExtensionId));

  // The user logs in. The app extension disabled, although still installed.
  LogIn();
  EXPECT_TRUE(IsExtensionInstalled(kGoodExtensionId));
  EXPECT_FALSE(IsExtensionEnabled(kGoodExtensionId));

  // The user locks the session. The extension gets enabled.
  extensions::TestExtensionRegistryObserver lock_screen_observer(
      extensions::ExtensionRegistry::Get(GetOriginalSigninProfile()),
      kGoodExtensionId);
  LockSession();
  lock_screen_observer.WaitForExtensionReady();
  ASSERT_TRUE(IsExtensionEnabled(kGoodExtensionId));
}

// Tests that the LoginScreenExtensionsContentScriptManager disables a login
// screen extension with content scripts that do not match the allowlisted
// URL.
class LoginScreenExtensionsBadContentScriptTest
    : public LoginScreenExtensionsGoodContentScriptTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    LoginScreenExtensionsGoodContentScriptTest::SetUpCommandLine(command_line);

    command_line->AppendSwitchASCII(
        extensions::switches::kAllowlistedExtensionID, kBadExtensionId);
  }
};

// Tests that a force-installed extension with a disallowed content script is
// disabled on the login/lock screen.
IN_PROC_BROWSER_TEST_F(LoginScreenExtensionsBadContentScriptTest,
                       BadExtensionDisabled) {
  extensions::TestExtensionRegistryObserver login_screen_observer(
      extensions::ExtensionRegistry::Get(GetOriginalSigninProfile()),
      kBadExtensionId);
  EXPECT_TRUE(extension_force_install_mixin()->ForceInstallFromSourceDir(
      base::PathService::CheckedGet(chrome::DIR_TEST_DATA)
          .AppendASCII(kBadExtensionPath),
      base::PathService::CheckedGet(chrome::DIR_TEST_DATA)
          .AppendASCII(kBadExtensionPemPath),
      ExtensionForceInstallMixin::WaitMode::kLoad));
  login_screen_observer.WaitForExtensionUnloaded();
  EXPECT_TRUE(IsExtensionInstalled(kBadExtensionId));
  EXPECT_FALSE(IsExtensionEnabled(kBadExtensionId));

  // The user logs in. The extension is still disabled.
  LogIn();
  EXPECT_TRUE(IsExtensionInstalled(kBadExtensionId));
  EXPECT_FALSE(IsExtensionEnabled(kBadExtensionId));

  // The user locks the session. The extension is still disabled.
  extensions::TestExtensionRegistryObserver lock_screen_observer(
      extensions::ExtensionRegistry::Get(GetOriginalSigninProfile()),
      kBadExtensionId);
  LockSession();
  lock_screen_observer.WaitForExtensionUnloaded();
  EXPECT_TRUE(IsExtensionInstalled(kBadExtensionId));
  EXPECT_FALSE(IsExtensionEnabled(kBadExtensionId));
}

}  // namespace ash
