// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/extensions/authentication_screen_extensions_external_loader.h"

#include "ash/constants/ash_switches.h"
#include "base/path_service.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/login/lock/screen_locker_tester.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/policy/extension_force_install_mixin.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_types.h"
#include "chromeos/constants/chromeos_features.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/common/switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {

// A test extension used to simulate the presence of the production
// "Identity Card Connector" extension.
constexpr char kBadgeAuthExtensionId[] = "aimmmpohpbmiljechlemfgeioaoknfnm";
constexpr char kBadgeAuthExtensionDirPath[] =
    "extensions/auth_screen_external_loader/extension/";
constexpr char kBadgeAuthExtensionPemPath[] =
    "extensions/auth_screen_external_loader/extension.pem";

// A generic test extension which represents any other force-installed extension
// that doesn't enable the Badge Based Authentication flow. Already allowlisted
// for the sign-in profile.
constexpr char kOtherExtensionId[] = "oclffehlkdgibkainkilopaalpdobkan";
constexpr char kOtherExtensionCrx[] =
    "extensions/api_test/login_screen_apis/extension.crx";

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

class AuthenticationScreenExtensionsExternalLoaderBrowserTest
    : public MixinBasedInProcessBrowserTest {
 public:
  AuthenticationScreenExtensionsExternalLoaderBrowserTest(
      const AuthenticationScreenExtensionsExternalLoaderBrowserTest&) = delete;
  AuthenticationScreenExtensionsExternalLoaderBrowserTest& operator=(
      const AuthenticationScreenExtensionsExternalLoaderBrowserTest&) = delete;

 protected:
  AuthenticationScreenExtensionsExternalLoaderBrowserTest() {
    // Don't shut down when no browser is open, since it breaks the test and
    // since it's not the real Chrome OS behavior.
    set_exit_when_last_browser_closes(false);
  }

  ~AuthenticationScreenExtensionsExternalLoaderBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(ash::switches::kOobeSkipPostLogin);

    command_line->AppendSwitchASCII(
        extensions::switches::kAllowlistedExtensionID, kBadgeAuthExtensionId);
    MixinBasedInProcessBrowserTest::SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
    extension_force_install_mixin_.InitWithDeviceStateMixin(
        GetOriginalSigninProfile(), &device_state_mixin_);
  }

  void EnsureLockProfileExists() {
    if (ash::BrowserContextHelper::Get()->GetLockScreenBrowserContext()) {
      return;
    }

    base::test::TestFuture<Profile*> profile_future;
    g_browser_process->profile_manager()->CreateProfileAsync(
        ash::ProfileHelper::GetLockScreenProfileDir(),
        profile_future.GetCallback());
    CHECK(profile_future.Get()) << "Lock profile wasn't created";
  }

  void LogIn() {
    login_manager_mixin_.LoginAsNewRegularUser();
    login_manager_mixin_.WaitForActiveSession();
  }

  void LockSession() { ash::ScreenLockerTester().Lock(); }

  void UnlockSession() {
    const std::string kPassword = "pass";
    const AccountId account_id =
        user_manager::UserManager::Get()->GetPrimaryUser()->GetAccountId();
    ash::ScreenLockerTester screen_locker_tester;
    screen_locker_tester.SetUnlockPassword(account_id, kPassword);
    screen_locker_tester.UnlockWithPassword(account_id, kPassword);
    screen_locker_tester.WaitForUnlock();
  }

  ExtensionForceInstallMixin* extension_force_install_mixin() {
    return &extension_force_install_mixin_;
  }

  bool IsExtensionInstalledOnSigninScreen(
      const std::string& extension_id) const {
    return extension_force_install_mixin_.GetInstalledExtension(extension_id) !=
           nullptr;
  }

  bool IsExtensionEnabledOnSigninScreen(const std::string& extension_id) const {
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

  // Helper to install the test extensions.
  // If `enable_badge_auth` is true, kBadgeAuthExtensionId is installed,
  // making the Badge Based Auth feature active. kOtherExtensionId is always
  // installed.
  void InstallTestExtensions(bool enable_badge_auth) {
    AuthenticationScreenExtensionsExternalLoader::
        SetTestBadgeAuthExtensionIdForTesting(kBadgeAuthExtensionId);

    // Install the Badge Based Auth extension if enabled.
    if (enable_badge_auth) {
      ASSERT_TRUE(extension_force_install_mixin()->ForceInstallFromSourceDir(
          base::PathService::CheckedGet(chrome::DIR_TEST_DATA)
              .AppendASCII(kBadgeAuthExtensionDirPath),
          base::PathService::CheckedGet(chrome::DIR_TEST_DATA)
              .AppendASCII(kBadgeAuthExtensionPemPath),
          ExtensionForceInstallMixin::WaitMode::kLoad));
      ASSERT_TRUE(IsExtensionInstalledOnSigninScreen(kBadgeAuthExtensionId));
      ASSERT_TRUE(IsExtensionEnabledOnSigninScreen(kBadgeAuthExtensionId));
    }

    // Always install the generic extension.
    extensions::ExtensionId other_ext_id;
    ASSERT_TRUE(extension_force_install_mixin()->ForceInstallFromCrx(
        base::PathService::CheckedGet(chrome::DIR_TEST_DATA)
            .AppendASCII(kOtherExtensionCrx),
        ExtensionForceInstallMixin::WaitMode::kLoad, &other_ext_id));
    ASSERT_EQ(other_ext_id, kOtherExtensionId);
    ASSERT_TRUE(IsExtensionInstalledOnSigninScreen(kOtherExtensionId));
    ASSERT_TRUE(IsExtensionEnabledOnSigninScreen(kOtherExtensionId));
  }

  base::test::ScopedFeatureList scoped_feature_list_{
      chromeos::features::kLockScreenBadgeAuth};
  ash::DeviceStateMixin device_state_mixin_{
      &mixin_host_,
      ash::DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};
  ash::LoginManagerMixin login_manager_mixin_{&mixin_host_};
  ExtensionForceInstallMixin extension_force_install_mixin_{&mixin_host_};
};

// Tests that all sign-in screen extensions remain loaded on the sign-in profile
// when the session is locked but the lock screen profile has not been
// created.
IN_PROC_BROWSER_TEST_F(AuthenticationScreenExtensionsExternalLoaderBrowserTest,
                       ExtensionsStayOnSignin_NoLockProfile) {
  InstallTestExtensions(/*enable_badge_auth=*/true);

  LogIn();
  LockSession();
  // Even if the session is locked, the lock screen profile doesn't get created
  // unless needed.
  ASSERT_FALSE(ash::BrowserContextHelper::Get()->GetLockScreenBrowserContext());
  EXPECT_TRUE(IsExtensionInstalledOnSigninScreen(kBadgeAuthExtensionId));
  EXPECT_TRUE(IsExtensionEnabledOnSigninScreen(kBadgeAuthExtensionId));
  EXPECT_TRUE(IsExtensionInstalledOnSigninScreen(kOtherExtensionId));
  EXPECT_TRUE(IsExtensionEnabledOnSigninScreen(kOtherExtensionId));
}

// Tests that all sign-in screen extensions remain loaded on the sign-in profile
// when the session is locked but Badge Based Authentication isn't enabled.
IN_PROC_BROWSER_TEST_F(AuthenticationScreenExtensionsExternalLoaderBrowserTest,
                       ExtensionsStayOnSignin_BadgeAuthNotActive) {
  InstallTestExtensions(/*enable_badge_auth=*/false);

  LogIn();
  EnsureLockProfileExists();
  LockSession();

  // Even though the lock profile exists, "ICC" isn't enabled so the extension
  // remains on the sign-in profile.
  EXPECT_TRUE(IsExtensionInstalledOnSigninScreen(kOtherExtensionId));
  EXPECT_TRUE(IsExtensionEnabledOnSigninScreen(kOtherExtensionId));

  EXPECT_FALSE(IsExtensionInstalledOnLockScreen(kOtherExtensionId));
  EXPECT_FALSE(IsExtensionInstalledOnLockScreen(kOtherExtensionId));
}

// Tests that all sign-in screen extensions are unloaded from the sign-in
// profile when the session is locked if the lock screen profile already exists
// and Badge Based Auth is enabled.
IN_PROC_BROWSER_TEST_F(AuthenticationScreenExtensionsExternalLoaderBrowserTest,
                       ExtensionsMoveToLockScreen_BadgeAuthActive) {
  InstallTestExtensions(/*enable_badge_auth=*/true);

  LogIn();
  EnsureLockProfileExists();

  extensions::TestExtensionRegistryObserver signin_observer_badge_unload(
      extensions::ExtensionRegistry::Get(GetOriginalSigninProfile()),
      kBadgeAuthExtensionId);
  extensions::TestExtensionRegistryObserver signin_observer_other_unload(
      extensions::ExtensionRegistry::Get(GetOriginalSigninProfile()),
      kOtherExtensionId);

  extensions::TestExtensionRegistryObserver lock_observer_badge_load(
      extensions::ExtensionRegistry::Get(GetOriginalLockScreenProfile()),
      kBadgeAuthExtensionId);
  extensions::TestExtensionRegistryObserver lock_observer_other_load(
      extensions::ExtensionRegistry::Get(GetOriginalLockScreenProfile()),
      kOtherExtensionId);
  LockSession();
  if (!IsExtensionInstalledOnLockScreen(kBadgeAuthExtensionId)) {
    lock_observer_badge_load.WaitForExtensionLoaded();
  }
  if (!IsExtensionInstalledOnLockScreen(kOtherExtensionId)) {
    lock_observer_other_load.WaitForExtensionLoaded();
  }
  if (IsExtensionInstalledOnSigninScreen(kBadgeAuthExtensionId)) {
    signin_observer_badge_unload.WaitForExtensionUnloaded();
  }
  if (IsExtensionInstalledOnSigninScreen(kOtherExtensionId)) {
    signin_observer_other_unload.WaitForExtensionUnloaded();
  }

  EXPECT_FALSE(IsExtensionInstalledOnSigninScreen(kBadgeAuthExtensionId));
  EXPECT_FALSE(IsExtensionInstalledOnSigninScreen(kOtherExtensionId));

  EXPECT_TRUE(IsExtensionInstalledOnLockScreen(kBadgeAuthExtensionId));
  EXPECT_TRUE(IsExtensionEnabledOnLockScreen(kBadgeAuthExtensionId));
  EXPECT_TRUE(IsExtensionInstalledOnLockScreen(kOtherExtensionId));
  EXPECT_TRUE(IsExtensionEnabledOnLockScreen(kOtherExtensionId));
}

}  // namespace chromeos
