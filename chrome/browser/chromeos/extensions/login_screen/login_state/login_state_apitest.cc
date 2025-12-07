// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_switches.h"
#include "base/path_service.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/extensions/authentication_screen_extensions_external_loader.h"
#include "chrome/browser/ash/login/lock/screen_locker_tester.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/extensions/login_screen/login_state/login_state_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/policy/extension_force_install_mixin.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/constants/chromeos_features.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/api/test/test_api.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/common/switches.h"
#include "extensions/test/result_catcher.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {
constexpr char kExtensionId[] = "fodcoebjjjgpdglcdkfmkfcplecoicbe";
constexpr char kExtensionPath[] =
    "extensions/api_test/login_screen_apis/login_state/get_profile_type/";
constexpr char kExtensionPemPath[] =
    "extensions/api_test/login_screen_apis/login_state/get_profile_type.pem";

// Returns the profile into which login-screen extensions are force-installed.
Profile* GetOriginalSigninProfile() {
  return Profile::FromBrowserContext(
             ash::BrowserContextHelper::Get()->GetSigninBrowserContext())
      ->GetOriginalProfile();
}

// Returns the profile into which lock-screen extensions are force-installed.
Profile* GetOriginalLockScreenProfile() {
  return Profile::FromBrowserContext(
             ash::BrowserContextHelper::Get()->GetLockScreenBrowserContext())
      ->GetOriginalProfile();
}
}  // namespace

using LoginStateApitest = ExtensionApiTest;

// Test that `loginState.getProfileType()` returns `USER_PROFILE` for
// extensions not running in the signin profile.
IN_PROC_BROWSER_TEST_F(LoginStateApitest, GetProfileType_UserProfile) {
  EXPECT_TRUE(RunExtensionTest("login_screen_apis/login_state/get_profile_type",
                               {.custom_arg = "USER_PROFILE"}));
}

// Test that `loginState.getSessionState()` returns `IN_SESSION` for extensions
// not running on the login screen.
IN_PROC_BROWSER_TEST_F(LoginStateApitest, GetSessionState_InSession) {
  EXPECT_TRUE(
      RunExtensionTest("login_screen_apis/login_state/get_session_state",
                       {.custom_arg = "IN_SESSION"}));
}

class AuthenticationScreenLoginStateApiTest
    : public MixinBasedInProcessBrowserTest {
 public:
  AuthenticationScreenLoginStateApiTest(
      const AuthenticationScreenLoginStateApiTest&) = delete;
  AuthenticationScreenLoginStateApiTest& operator=(
      const AuthenticationScreenLoginStateApiTest&) = delete;

 protected:
  AuthenticationScreenLoginStateApiTest() {
    // Don't shut down when no browser is open, since it breaks the test (lock
    // profile can't get created) and since it's not the real Chrome OS
    // behavior.
    set_exit_when_last_browser_closes(false);
  }
  ~AuthenticationScreenLoginStateApiTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Skip showing post-login screens.
    command_line->AppendSwitch(ash::switches::kOobeSkipPostLogin);
    command_line->AppendSwitchASCII(switches::kAllowlistedExtensionID,
                                    kExtensionId);

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

  ExtensionForceInstallMixin* extension_force_install_mixin() {
    return &extension_force_install_mixin_;
  }

  bool IsExtensionInstalledOnLockScreen(const std::string& extension_id) const {
    const auto* const registry =
        extensions::ExtensionRegistry::Get(GetOriginalLockScreenProfile());
    CHECK(registry);
    return registry->GetInstalledExtension(extension_id) != nullptr;
  }

  void SetCustomTestArg(const std::string custom_arg) {
    config_.Set("customArg", custom_arg);
    extensions::TestGetConfigFunction::set_test_config_state(&config_);
  }

  base::test::ScopedFeatureList scoped_feature_list_{
      chromeos::features::kLockScreenBadgeAuth};
  ash::DeviceStateMixin device_state_mixin_{
      &mixin_host_,
      ash::DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};
  ash::LoginManagerMixin login_manager_mixin_{&mixin_host_};
  ExtensionForceInstallMixin extension_force_install_mixin_{&mixin_host_};
  base::Value::Dict config_;
};

// Test that `loginState.getProfileType()` returns `SIGNIN_PROFILE` for
// extensions running in the signin profile.
IN_PROC_BROWSER_TEST_F(AuthenticationScreenLoginStateApiTest,
                       GetProfileType_SigninProfile) {
  SetCustomTestArg("SIGNIN_PROFILE");
  ResultCatcher catcher;

  EXPECT_TRUE(extension_force_install_mixin()->ForceInstallFromSourceDir(
      base::PathService::CheckedGet(chrome::DIR_TEST_DATA)
          .AppendASCII(kExtensionPath),
      base::PathService::CheckedGet(chrome::DIR_TEST_DATA)
          .AppendASCII(kExtensionPemPath),
      ExtensionForceInstallMixin::WaitMode::kLoad));

  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

// Test that `loginState.getProfileType()` returns `LOCK_PROFILE` for
// extensions running in the lock profile.
IN_PROC_BROWSER_TEST_F(AuthenticationScreenLoginStateApiTest,
                       GetProfileType_LockProfile) {
  chromeos::AuthenticationScreenExtensionsExternalLoader::
      SetTestBadgeAuthExtensionIdForTesting(kExtensionId);

  SetCustomTestArg("LOCK_PROFILE");
  ResultCatcher catcher;

  LogIn();
  // Ensure that the Lock Screen profile exists.
  base::test::TestFuture<Profile*> profile_future;
  g_browser_process->profile_manager()->CreateProfileAsync(
      ash::ProfileHelper::GetLockScreenProfileDir(),
      profile_future.GetCallback());
  ASSERT_TRUE(profile_future.Take());

  ash::ScreenLockerTester().Lock();

  // "Force install" on the sign-in profile as the same policy is applied to the
  // lock screen profile, but don't wait as it won't actually be installed on
  // the sign-in profile.
  extensions::TestExtensionRegistryObserver observer(
      extensions::ExtensionRegistry::Get(GetOriginalLockScreenProfile()),
      kExtensionId);
  EXPECT_TRUE(extension_force_install_mixin()->ForceInstallFromSourceDir(
      base::PathService::CheckedGet(chrome::DIR_TEST_DATA)
          .AppendASCII(kExtensionPath),
      base::PathService::CheckedGet(chrome::DIR_TEST_DATA)
          .AppendASCII(kExtensionPemPath),
      ExtensionForceInstallMixin::WaitMode::kNone));
  if (!IsExtensionInstalledOnLockScreen(kExtensionId)) {
    observer.WaitForExtensionLoaded();
  }

  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}
}  // namespace extensions
