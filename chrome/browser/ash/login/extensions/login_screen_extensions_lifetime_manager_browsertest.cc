// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/extensions/login_screen_extensions_lifetime_manager.h"

#include <string>

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/path_service.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/extensions/authentication_screen_extensions_external_loader.h"
#include "chrome/browser/ash/login/lock/screen_locker_tester.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/extension_force_install_mixin.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_types.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/extension_host_test_helper.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/common/mojom/view_type.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

constexpr char kAllowlistedAppId[] = "bjaiihebfngildkcjkjckolinodhliff";
constexpr char kAllowlistedAppCrxPath[] =
    "extensions/signin_screen_manual_test_app/app_signed_by_webstore.crx";

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

class LoginScreenExtensionsLifetimeManagerTest
    : public MixinBasedInProcessBrowserTest {
 public:
  LoginScreenExtensionsLifetimeManagerTest(
      const LoginScreenExtensionsLifetimeManagerTest&) = delete;
  LoginScreenExtensionsLifetimeManagerTest& operator=(
      const LoginScreenExtensionsLifetimeManagerTest&) = delete;

 protected:
  LoginScreenExtensionsLifetimeManagerTest() {
    // Don't shut down when no browser is open, since it breaks the test and
    // since it's not the real Chrome OS behavior.
    set_exit_when_last_browser_closes(false);
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{chromeos::features::kLockScreenBadgeAuth},
        /*disabled_features=*/{});
  }

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

  bool IsExtensionInstalledOnLockScreen(const std::string& extension_id) const {
    const auto* const registry =
        extensions::ExtensionRegistry::Get(GetOriginalLockScreenProfile());
    CHECK(registry);
    return registry->GetInstalledExtension(extension_id) != nullptr;
  }

  bool IsExtensionEnabledOnLockScreen(const std::string& extension_id) const {
    const auto* const registry =
        extensions::ExtensionRegistry::Get(GetOriginalLockScreenProfile());
    CHECK(registry);
    return registry->enabled_extensions().GetByID(extension_id) != nullptr;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
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
      ExtensionForceInstallMixin::WaitMode::kBackgroundPageFirstLoad));
  ASSERT_TRUE(IsExtensionEnabled(kAllowlistedAppId));

  // The user logs in. The app gets disabled, although still installed.
  LogIn();
  EXPECT_TRUE(IsExtensionInstalled(kAllowlistedAppId));
  EXPECT_FALSE(IsExtensionEnabled(kAllowlistedAppId));

  // The user locks the session. The app gets enabled and the background page
  // is loaded again.
  {
    extensions::ExtensionHostTestHelper background_ready(
        GetOriginalSigninProfile(), kAllowlistedAppId);
    background_ready.RestrictToType(
        extensions::mojom::ViewType::kExtensionBackgroundPage);
    LockSession();
    background_ready.WaitForHostCompletedFirstLoad();
    ASSERT_TRUE(IsExtensionEnabled(kAllowlistedAppId));
  }

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
  // installed, but almost immediately (in an async job) gets disabled.
  LogIn();
  EXPECT_TRUE(extension_force_install_mixin()->ForceInstallFromCrx(
      base::PathService::CheckedGet(chrome::DIR_TEST_DATA)
          .AppendASCII(kAllowlistedAppCrxPath),
      ExtensionForceInstallMixin::WaitMode::kLoad));
  EXPECT_TRUE(IsExtensionInstalled(kAllowlistedAppId));
  // Wait until the extension gets disabled. Sadly, the extensions system
  // doesn't provide a proper way to wait until that happens, so we're relying
  // on the production code doing this in zero-delayed async job.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(IsExtensionEnabled(kAllowlistedAppId));

  // The user locks the session. The app gets enabled and the background page is
  // loaded again.
  extensions::ExtensionHostTestHelper background_ready(
      GetOriginalSigninProfile(), kAllowlistedAppId);
  background_ready.RestrictToType(
      extensions::mojom::ViewType::kExtensionBackgroundPage);
  LockSession();
  background_ready.WaitForHostCompletedFirstLoad();
  ASSERT_TRUE(IsExtensionEnabled(kAllowlistedAppId));
}

// Tests that an extension force-installed on the lock screen profile gets
// disabled during an active user session.
IN_PROC_BROWSER_TEST_F(LoginScreenExtensionsLifetimeManagerTest, LockScreen) {
  chromeos::AuthenticationScreenExtensionsExternalLoader::
      SetTestBadgeAuthExtensionIdForTesting(kAllowlistedAppId);

  // The user logs in.
  LogIn();

  // Ensure that the lock profile exits.
  base::test::TestFuture<Profile*> profile_future;
  g_browser_process->profile_manager()->CreateProfileAsync(
      ash::ProfileHelper::GetLockScreenProfileDir(),
      profile_future.GetCallback());
  ASSERT_TRUE(profile_future.Get()) << "Lock profile wasn't created";

  // The user locks the session. The app gets enabled and the background page
  // is loaded.
  {
    extensions::TestExtensionRegistryObserver load_observer(
        extensions::ExtensionRegistry::Get(GetOriginalLockScreenProfile()),
        kAllowlistedAppId);
    extensions::ExtensionHostTestHelper background_ready(
        GetOriginalLockScreenProfile(), kAllowlistedAppId);
    background_ready.RestrictToType(
        extensions::mojom::ViewType::kExtensionBackgroundPage);
    LockSession();

    // Force-install the app while on the lock screen. Since badge based auth is
    // enabled it won't get installed on the login profile, so skip waiting.
    EXPECT_TRUE(extension_force_install_mixin()->ForceInstallFromCrx(
        base::PathService::CheckedGet(chrome::DIR_TEST_DATA)
            .AppendASCII(kAllowlistedAppCrxPath),
        ExtensionForceInstallMixin::WaitMode::kNone));

    background_ready.WaitForHostCompletedFirstLoad();
    ASSERT_TRUE(IsExtensionEnabledOnLockScreen(kAllowlistedAppId));
  }

  // The user unlocks the session. The app gets disabled.
  UnlockSession();
  EXPECT_FALSE(IsExtensionInstalledOnLockScreen(kAllowlistedAppId));
  EXPECT_FALSE(IsExtensionEnabledOnLockScreen(kAllowlistedAppId));
}

}  // namespace ash
