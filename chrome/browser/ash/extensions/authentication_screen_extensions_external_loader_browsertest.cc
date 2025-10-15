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
#include "chrome/browser/ash/login/test/session_manager_state_waiter.h"
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

constexpr char kExtensionId[] = "aimmmpohpbmiljechlemfgeioaoknfnm";
constexpr char kExtensionDirPath[] =
    "extensions/auth_screen_external_loader/extension/";
constexpr char kExtensionPemPath[] =
    "extensions/auth_screen_external_loader/extension.pem";

// An extension that's already allowlisted for the sign-in profile.
constexpr char kOtherExtensionId[] = "oclffehlkdgibkainkilopaalpdobkan";
constexpr char kOtherExtensionCrx[] =
    "extensions/api_test/login_screen_apis/extension.crx";

// Returns the profile into which login-screen extensions are force-installed.
Profile* GetOriginalSigninProfile() {
  return Profile::FromBrowserContext(
             ash::BrowserContextHelper::Get()->GetSigninBrowserContext())
      ->GetOriginalProfile();
}

// Returns the profile into which login-screen extensions are force-installed.
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
        extensions::switches::kAllowlistedExtensionID, kExtensionId);
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

  base::test::ScopedFeatureList scoped_feature_list_{
      chromeos::features::kLockScreenBadgeAuth};
  ash::DeviceStateMixin device_state_mixin_{
      &mixin_host_,
      ash::DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};
  ash::LoginManagerMixin login_manager_mixin_{&mixin_host_};
  ExtensionForceInstallMixin extension_force_install_mixin_{&mixin_host_};
};

// Tests that sign-in screen extensions remain loaded when the session is locked
// if the lock screen profile has not yet been created.
IN_PROC_BROWSER_TEST_F(AuthenticationScreenExtensionsExternalLoaderBrowserTest,
                       NotUnloadedWhenNoLockProfile) {
  AuthenticationScreenExtensionsExternalLoader::
      SetTestBadgeAuthExtensionIdForTesting(kExtensionId);

  EXPECT_TRUE(extension_force_install_mixin()->ForceInstallFromSourceDir(
      base::PathService::CheckedGet(chrome::DIR_TEST_DATA)
          .AppendASCII(kExtensionDirPath),
      base::PathService::CheckedGet(chrome::DIR_TEST_DATA)
          .AppendASCII(kExtensionPemPath),
      ExtensionForceInstallMixin::WaitMode::kLoad));
  EXPECT_TRUE(IsExtensionInstalledOnSigninScreen(kExtensionId));
  EXPECT_TRUE(IsExtensionEnabledOnSigninScreen(kExtensionId));

  LogIn();
  ash::SessionStateWaiter locked_waiter(session_manager::SessionState::LOCKED);
  LockSession();
  locked_waiter.Wait();
  // Even if the session is locked, the lock screen profile doesn't get created
  // unless needed.
  ASSERT_FALSE(ash::BrowserContextHelper::Get()->GetLockScreenBrowserContext());
  EXPECT_TRUE(IsExtensionInstalledOnSigninScreen(kExtensionId));
  EXPECT_TRUE(IsExtensionEnabledOnSigninScreen(kExtensionId));
}

// Tests that sign-in screen extensions remain loaded when the session is locked
// but Badge Based Authentication isn't enabled.
IN_PROC_BROWSER_TEST_F(AuthenticationScreenExtensionsExternalLoaderBrowserTest,
                       NotUnloadedWhenBadgeBasedAuthNotEnabled) {
  EXPECT_TRUE(extension_force_install_mixin()->ForceInstallFromSourceDir(
      base::PathService::CheckedGet(chrome::DIR_TEST_DATA)
          .AppendASCII(kExtensionDirPath),
      base::PathService::CheckedGet(chrome::DIR_TEST_DATA)
          .AppendASCII(kExtensionPemPath),
      ExtensionForceInstallMixin::WaitMode::kLoad));
  EXPECT_TRUE(IsExtensionInstalledOnSigninScreen(kExtensionId));
  EXPECT_TRUE(IsExtensionEnabledOnSigninScreen(kExtensionId));

  LogIn();
  EnsureLockProfileExists();
  ash::SessionStateWaiter locked_waiter(session_manager::SessionState::LOCKED);
  LockSession();
  locked_waiter.Wait();

  // Even though the lock profile exists, "ICC" isn't enabled so the extension
  // remains on the sign-in profile.
  EXPECT_TRUE(IsExtensionInstalledOnSigninScreen(kExtensionId));
  EXPECT_TRUE(IsExtensionEnabledOnSigninScreen(kExtensionId));
}

// Tests that sign-in screen extensions are unloaded when the session is locked
// if the lock screen profile already exists and Badge Based Auth is enabled.
IN_PROC_BROWSER_TEST_F(AuthenticationScreenExtensionsExternalLoaderBrowserTest,
                       UnloadedWhenLockProfileExists) {
  AuthenticationScreenExtensionsExternalLoader::
      SetTestBadgeAuthExtensionIdForTesting(kExtensionId);

  EXPECT_TRUE(extension_force_install_mixin()->ForceInstallFromSourceDir(
      base::PathService::CheckedGet(chrome::DIR_TEST_DATA)
          .AppendASCII(kExtensionDirPath),
      base::PathService::CheckedGet(chrome::DIR_TEST_DATA)
          .AppendASCII(kExtensionPemPath),
      ExtensionForceInstallMixin::WaitMode::kLoad));
  EXPECT_TRUE(IsExtensionInstalledOnSigninScreen(kExtensionId));
  EXPECT_TRUE(IsExtensionEnabledOnSigninScreen(kExtensionId));

  LogIn();
  EnsureLockProfileExists();
  ash::SessionStateWaiter locked_waiter(session_manager::SessionState::LOCKED);
  LockSession();
  locked_waiter.Wait();

  extensions::TestExtensionRegistryObserver observer(
      extensions::ExtensionRegistry::Get(GetOriginalLockScreenProfile()),
      kExtensionId);
  observer.WaitForExtensionLoaded();

  EXPECT_FALSE(IsExtensionInstalledOnSigninScreen(kExtensionId));
  EXPECT_TRUE(IsExtensionInstalledOnLockScreen(kExtensionId));
  EXPECT_TRUE(IsExtensionEnabledOnLockScreen(kExtensionId));

  ash::SessionStateWaiter active_waiter(session_manager::SessionState::ACTIVE);
  UnlockSession();
  active_waiter.Wait();
  observer.WaitForExtensionUnloaded();
  EXPECT_FALSE(IsExtensionInstalledOnLockScreen(kExtensionId));
}

// Tests that all sign-in screen extensions are moved between the sign-in and
// lock profiles when Badge Based Auth is enabled/disabled.
IN_PROC_BROWSER_TEST_F(AuthenticationScreenExtensionsExternalLoaderBrowserTest,
                       ExtensionsMoveToLockScreenOnBadgeAuthEnabled) {
  AuthenticationScreenExtensionsExternalLoader::
      SetTestBadgeAuthExtensionIdForTesting(kExtensionId);

  extensions::ExtensionId extension_id;
  EXPECT_TRUE(extension_force_install_mixin()->ForceInstallFromCrx(
      base::PathService::CheckedGet(chrome::DIR_TEST_DATA)
          .AppendASCII(kOtherExtensionCrx),
      ExtensionForceInstallMixin::WaitMode::kLoad, &extension_id));
  EXPECT_TRUE(IsExtensionInstalledOnSigninScreen(kOtherExtensionId));
  EXPECT_TRUE(IsExtensionEnabledOnSigninScreen(kOtherExtensionId));

  LogIn();
  EnsureLockProfileExists();
  ash::SessionStateWaiter locked_waiter_1(
      session_manager::SessionState::LOCKED);
  LockSession();
  locked_waiter_1.Wait();
  // The extension stays on the sign-in profile before Badge Auth is enabled.
  EXPECT_TRUE(IsExtensionInstalledOnSigninScreen(kOtherExtensionId));
  EXPECT_FALSE(IsExtensionInstalledOnLockScreen(kOtherExtensionId));

  ash::SessionStateWaiter active_waiter(session_manager::SessionState::ACTIVE);
  UnlockSession();
  active_waiter.Wait();

  // Install the Badge Auth extension to enable the feature.
  EXPECT_TRUE(extension_force_install_mixin()->ForceInstallFromSourceDir(
      base::PathService::CheckedGet(chrome::DIR_TEST_DATA)
          .AppendASCII(kExtensionDirPath),
      base::PathService::CheckedGet(chrome::DIR_TEST_DATA)
          .AppendASCII(kExtensionPemPath),
      ExtensionForceInstallMixin::WaitMode::kLoad));
  EXPECT_TRUE(IsExtensionInstalledOnSigninScreen(kExtensionId));

  extensions::TestExtensionRegistryObserver observer(
      extensions::ExtensionRegistry::Get(GetOriginalLockScreenProfile()),
      kExtensionId);
  extensions::TestExtensionRegistryObserver other_observer(
      extensions::ExtensionRegistry::Get(GetOriginalLockScreenProfile()),
      kOtherExtensionId);

  ash::SessionStateWaiter locked_waiter_2(
      session_manager::SessionState::LOCKED);
  ash::ScreenLockerTester().Lock();
  locked_waiter_2.Wait();

  // Both extensions are loaded on the lock screen profile.
  if (!IsExtensionInstalledOnLockScreen(kExtensionId)) {
    observer.WaitForExtensionLoaded();
  }
  if (!IsExtensionInstalledOnLockScreen(kOtherExtensionId)) {
    other_observer.WaitForExtensionLoaded();
  }

  EXPECT_FALSE(IsExtensionInstalledOnSigninScreen(kExtensionId));
  EXPECT_TRUE(IsExtensionInstalledOnLockScreen(kExtensionId));

  EXPECT_FALSE(IsExtensionInstalledOnSigninScreen(kOtherExtensionId));
  EXPECT_TRUE(IsExtensionInstalledOnLockScreen(kOtherExtensionId));
}

}  // namespace chromeos
