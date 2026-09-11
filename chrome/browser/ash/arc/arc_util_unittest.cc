// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/arc_util.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/system/sys_info.h"
#include "base/test/icu_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/browser/ash/login/oobe_configuration.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/policy/core/device_local_account.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/login/fake_login_display_host.h"
#include "chrome/browser/ui/webui/ash/login/consolidated_consent_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/demo_preferences_screen_handler.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/experiences/arc/arc_features.h"
#include "chromeos/ash/experiences/arc/arc_prefs.h"
#include "components/account_id/account_id.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/session_manager/test/test_user_session_manager.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_names.h"
#include "components/version_info/version_info.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/gaia/gaia_id.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {
namespace util {

namespace {

constexpr char kTestProfileName[] = "user@gmail.com";
constexpr GaiaId::Literal kTestGaiaId("1234567890");

void SetProfileIsManagedForTesting(Profile* profile) {
  policy::ProfilePolicyConnector* const connector =
      profile->GetProfilePolicyConnector();
  connector->OverrideIsManagedForTesting(true);
}

void DisableDBusForProfileManager() {
  // Prevent access to DBus. This switch is reset in case set from test SetUp
  // due massive usage of InitFromArgv.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(switches::kTestType))
    command_line->AppendSwitch(switches::kTestType);
}

bool IsArcAllowedForProfileOnFirstCall(const Profile* profile) {
  ResetArcAllowedCheckForTesting(profile);
  return IsArcAllowedForProfile(profile);
}

}  // namespace

class ChromeArcUtilTest : public testing::Test {
 public:
  ChromeArcUtilTest() = default;

  ChromeArcUtilTest(const ChromeArcUtilTest&) = delete;
  ChromeArcUtilTest& operator=(const ChromeArcUtilTest&) = delete;

  ~ChromeArcUtilTest() override = default;

  void SetUp() override {
    command_line_ = std::make_unique<base::test::ScopedCommandLine>();
    command_line_->GetProcessCommandLine()->AppendSwitch(switches::kTestType);

    // TODO(crbug.com/278643115): Rework user/profile set up.
    ash::ProfileHelper::SetProfileToUserForTestingEnabled(true);
    test_user_session_manager_ =
        std::make_unique<ash::test::TestUserSessionManager>(
            TestingBrowserProcess::GetGlobal()->local_state());

    ASSERT_TRUE(data_dir_.CreateUniqueTempDir());
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());

    profile_ = profile_manager_->CreateTestingProfile(kTestProfileName);
  }

  void TearDown() override {
    // Avoid retries, let the next test start safely.
    ResetArcAllowedCheckForTesting(profile_);
    SetArcBlockedDueToIncompatibleFileSystemForTesting(false);
    profile_manager_->DeleteTestingProfile(kTestProfileName);
    profile_ = nullptr;
    profile_manager_.reset();
    test_user_session_manager_.reset();
    ash::ProfileHelper::SetProfileToUserForTestingEnabled(false);
    command_line_.reset();
  }

  TestingProfile* profile() { return profile_; }

  ash::test::TestUserSessionManager* test_user_session_manager() const {
    return test_user_session_manager_.get();
  }

 protected:
  ash::ScopedCrosSettingsTestHelper cros_settings_test_helper_;

 private:
  std::unique_ptr<base::test::ScopedCommandLine> command_line_;
  content::BrowserTaskEnvironment task_environment_;
  base::ScopedTempDir data_dir_;
  std::unique_ptr<ash::test::TestUserSessionManager> test_user_session_manager_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  // Owned by |profile_manager_|
  raw_ptr<TestingProfile, DanglingUntriaged> profile_ = nullptr;
};

TEST_F(ChromeArcUtilTest, IsArcAllowedForProfile) {
  // TODO(hidehiko): Fix profile and user login creation order.
  base::CommandLine::ForCurrentProcess()->InitFromArgv(
      {"", "--arc-availability=officially-supported"});
  const auto account_id = AccountId::FromUserEmailGaiaId(
      profile()->GetProfileUserName(), kTestGaiaId);
  CHECK(test_user_session_manager()->AddRegularUser(account_id));
  test_user_session_manager()->LogIn(account_id);
  EXPECT_TRUE(IsArcAllowedForProfileOnFirstCall(profile()));

  // false for nullptr.
  EXPECT_FALSE(IsArcAllowedForProfileOnFirstCall(nullptr));

  // false for incognito mode profile.
  EXPECT_FALSE(IsArcAllowedForProfileOnFirstCall(
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true)));
}

TEST_F(ChromeArcUtilTest, IsArcAllowedForProfileLegacy) {
  // TODO(hidehiko): Fix profile and user login creation order.
  base::CommandLine::ForCurrentProcess()->InitFromArgv({"", "--enable-arc"});
  const auto account_id = AccountId::FromUserEmailGaiaId(
      profile()->GetProfileUserName(), kTestGaiaId);
  CHECK(test_user_session_manager()->AddRegularUser(account_id));
  test_user_session_manager()->LogIn(account_id);
  EXPECT_TRUE(IsArcAllowedForProfileOnFirstCall(profile()));

  // false for nullptr.
  EXPECT_FALSE(IsArcAllowedForProfileOnFirstCall(nullptr));

  // false for incognito mode profile.
  EXPECT_FALSE(IsArcAllowedForProfileOnFirstCall(
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true)));
}

TEST_F(ChromeArcUtilTest, IsArcAllowedForProfile_DisableArc) {
  // TODO(hidehiko): Fix profile and user login creation order.
  base::CommandLine::ForCurrentProcess()->InitFromArgv({""});
  const auto account_id = AccountId::FromUserEmailGaiaId(
      profile()->GetProfileUserName(), kTestGaiaId);
  CHECK(test_user_session_manager()->AddRegularUser(account_id));
  test_user_session_manager()->LogIn(account_id);
  EXPECT_FALSE(IsArcAllowedForProfileOnFirstCall(profile()));
}

TEST_F(ChromeArcUtilTest, IsArcAllowedForProfile_NonPrimaryProfile) {
  // TODO(hidehiko): Fix profile and user login creation order.
  base::CommandLine::ForCurrentProcess()->InitFromArgv(
      {"", "--arc-availability=officially-supported"});
  const AccountId account_id2 =
      AccountId::FromUserEmailGaiaId("user2@gmail.com", GaiaId("0123456789"));
  const AccountId account_id = AccountId::FromUserEmailGaiaId(
      profile()->GetProfileUserName(), kTestGaiaId);
  CHECK(test_user_session_manager()->AddRegularUser(account_id2));
  CHECK(test_user_session_manager()->AddRegularUser(account_id));
  test_user_session_manager()->LogIn(account_id2);
  test_user_session_manager()->LogIn(account_id);
  EXPECT_FALSE(IsArcAllowedForProfileOnFirstCall(profile()));
}

// User without GAIA account.
TEST_F(ChromeArcUtilTest, IsArcAllowedForProfile_PublicAccount) {
  // TODO(hidehiko): Fix profile and user login creation order.
  base::CommandLine::ForCurrentProcess()->InitFromArgv(
      {"", "--arc-availability=officially-supported"});
  policy::DeviceLocalAccount device_local_account(
      policy::DeviceLocalAccountType::kPublicSession,
      policy::DeviceLocalAccount::EphemeralMode::kFollowDeviceWidePolicy,
      "public_user@gmail.com", /*kiosk_app_id=*/"",
      /*kiosk_app_update_url=*/"");
  CHECK(test_user_session_manager()->AddPublicAccountUser(
      device_local_account.user_id));
  test_user_session_manager()->LogIn(
      AccountId::FromUserEmail(device_local_account.user_id));
  EXPECT_TRUE(IsArcAllowedForProfile(profile()));
}

TEST_F(ChromeArcUtilTest, IsArcAllowedForProfile_GuestAccount) {
  // TODO(hidehiko): Fix profile and user login creation order.
  base::CommandLine::ForCurrentProcess()->InitFromArgv(
      {"", "--arc-availability=officially-supported"});
  CHECK(test_user_session_manager()->AddGuestUser());
  test_user_session_manager()->LogIn(user_manager::GuestAccountId());
  EXPECT_FALSE(IsArcAllowedForProfileOnFirstCall(profile()));
}

// Unmanaged account on managed device is not allowed to
// use arc on reven board.
TEST_F(ChromeArcUtilTest, IsArcAllowedForProfile_UnmanagedAccount_Reven) {
  base::CommandLine::ForCurrentProcess()->InitFromArgv(
      {"", "--arc-availability=officially-supported", "--reven-branding"});
  cros_settings_test_helper_.InstallAttributes()->SetCloudManaged(
      "example.com", "fake-device-id");
  EXPECT_FALSE(IsArcAllowedForProfileOnFirstCall(profile()));
}

// Managed account is allowed to use arc on reven board.
TEST_F(ChromeArcUtilTest, IsArcAllowedForProfile_ManagedDeviceAccount_Reven) {
  // TODO(hidehiko): Fix profile and user login creation order.
  base::CommandLine::ForCurrentProcess()->InitFromArgv(
      {"", "--arc-availability=officially-supported", "--reven-branding"});
  const auto account_id = AccountId::FromUserEmailGaiaId(
      profile()->GetProfileUserName(), kTestGaiaId);
  CHECK(test_user_session_manager()->AddRegularUser(account_id));
  test_user_session_manager()->LogIn(account_id);
  SetProfileIsManagedForTesting(profile());
  cros_settings_test_helper_.InstallAttributes()->SetCloudManaged(
      "example.com", "fake-device-id");
  EXPECT_TRUE(IsArcAllowedForProfileOnFirstCall(profile()));
}

TEST_F(ChromeArcUtilTest, IsArcBlockedDueToIncompatibleFileSystem_RegularUser) {
  // TODO(hidehiko): Fix profile and user login creation order.
  base::CommandLine::ForCurrentProcess()->InitFromArgv(
      {"", "--arc-availability=officially-supported"});
  SetArcBlockedDueToIncompatibleFileSystemForTesting(true);

  const AccountId user_id(AccountId::FromUserEmailGaiaId(
      profile()->GetProfileUserName(), kTestGaiaId));
  CHECK(test_user_session_manager()->AddRegularUser(user_id));
  test_user_session_manager()->LogIn(user_id);
  EXPECT_TRUE(IsArcBlockedDueToIncompatibleFileSystem(profile()));
}

TEST_F(ChromeArcUtilTest,
       IsArcBlockedDueToIncompatibleFileSystem_PublicAccount) {
  // TODO(hidehiko): Fix profile and user login creation order.
  base::CommandLine::ForCurrentProcess()->InitFromArgv(
      {"", "--arc-availability=officially-supported"});
  SetArcBlockedDueToIncompatibleFileSystemForTesting(true);

  policy::DeviceLocalAccount device_local_account(
      policy::DeviceLocalAccountType::kPublicSession,
      policy::DeviceLocalAccount::EphemeralMode::kFollowDeviceWidePolicy,
      "public_session", /*kiosk_app_id=*/"",
      /*kiosk_app_update_url=*/"");
  CHECK(test_user_session_manager()->AddPublicAccountUser(
      device_local_account.user_id));
  test_user_session_manager()->LogIn(
      AccountId::FromUserEmail(device_local_account.user_id));
  EXPECT_FALSE(IsArcBlockedDueToIncompatibleFileSystem(profile()));
}

TEST_F(ChromeArcUtilTest, IsArcCompatibleFileSystemUsedForProfile) {
  // TODO(hidehiko): Fix profile and user login creation order.
  base::CommandLine::ForCurrentProcess()->InitFromArgv(
      {"", "--arc-availability=officially-supported"});

  const AccountId id(AccountId::FromUserEmailGaiaId(
      profile()->GetProfileUserName(), kTestGaiaId));
  CHECK(test_user_session_manager()->AddRegularUser(id));
  test_user_session_manager()->LogIn(id);
  const user_manager::User* user =
      user_manager::UserManager::Get()->FindUser(id);

  // Unconfirmed
  EXPECT_TRUE(IsArcCompatibleFileSystemUsedForUser(user));

  user_manager::KnownUser known_user(g_browser_process->local_state());
  // Old FS
  known_user.SetIntegerPref(id, prefs::kArcCompatibleFilesystemChosen,
                            kFileSystemIncompatible);
  EXPECT_FALSE(IsArcCompatibleFileSystemUsedForUser(user));

  // New FS
  known_user.SetIntegerPref(id, prefs::kArcCompatibleFilesystemChosen,
                            kFileSystemCompatible);
  EXPECT_TRUE(IsArcCompatibleFileSystemUsedForUser(user));

  // New FS (User notified)
  known_user.SetIntegerPref(id, prefs::kArcCompatibleFilesystemChosen,
                            kFileSystemCompatibleAndNotifiedDeprecated);
  EXPECT_TRUE(IsArcCompatibleFileSystemUsedForUser(user));
}

TEST_F(ChromeArcUtilTest, ArcPlayStoreEnabledForProfile) {
  // TODO(hidehiko): Fix profile and user login creation order.
  base::CommandLine::ForCurrentProcess()->InitFromArgv(
      {"", "--arc-availability=officially-supported"});
  // Ensure IsAllowedForProfile() true.
  const auto account_id = AccountId::FromUserEmailGaiaId(
      profile()->GetProfileUserName(), kTestGaiaId);
  CHECK(test_user_session_manager()->AddRegularUser(account_id));
  test_user_session_manager()->LogIn(account_id);
  ASSERT_TRUE(IsArcAllowedForProfileOnFirstCall(profile()));

  // By default, Google Play Store is disabled.
  EXPECT_FALSE(IsArcPlayStoreEnabledForProfile(profile()));

  // Enable Google Play Store.
  SetArcPlayStoreEnabledForProfile(profile(), true);
  EXPECT_TRUE(IsArcPlayStoreEnabledForProfile(profile()));

  // Disable Google Play Store.
  SetArcPlayStoreEnabledForProfile(profile(), false);
  EXPECT_FALSE(IsArcPlayStoreEnabledForProfile(profile()));
}

TEST_F(ChromeArcUtilTest, ArcPlayStoreEnabledForProfile_NotAllowed) {
  base::CommandLine::ForCurrentProcess()->InitFromArgv(
      {"", "--arc-availability=officially-supported"});
  ASSERT_FALSE(IsArcAllowedForProfileOnFirstCall(profile()));

  // If ARC is not allowed for the profile, always return false.
  EXPECT_FALSE(IsArcPlayStoreEnabledForProfile(profile()));

  // Directly set the preference value, to avoid DCHECK in
  // SetArcPlayStoreEnabledForProfile().
  profile()->GetPrefs()->SetBoolean(prefs::kArcEnabled, true);
  EXPECT_FALSE(IsArcPlayStoreEnabledForProfile(profile()));
}

TEST_F(ChromeArcUtilTest, ArcPlayStoreEnabledForProfile_Managed) {
  // TODO(hidehiko): Fix profile and user login creation order.
  base::CommandLine::ForCurrentProcess()->InitFromArgv(
      {"", "--arc-availability=officially-supported"});
  // Ensure IsAllowedForProfile() true.
  const auto account_id = AccountId::FromUserEmailGaiaId(
      profile()->GetProfileUserName(), kTestGaiaId);
  CHECK(test_user_session_manager()->AddRegularUser(account_id));
  test_user_session_manager()->LogIn(account_id);
  ASSERT_TRUE(IsArcAllowedForProfileOnFirstCall(profile()));

  // By default it is not managed.
  EXPECT_FALSE(IsArcPlayStoreEnabledPreferenceManagedForProfile(profile()));
  EXPECT_FALSE(IsArcPlayStoreEnabledForProfile(profile()));

  // 1) Set managed preference to true, then try to set the value to false
  // via SetArcPlayStoreEnabledForProfile().
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kArcEnabled, std::make_unique<base::Value>(true));
  EXPECT_TRUE(IsArcPlayStoreEnabledPreferenceManagedForProfile(profile()));
  EXPECT_TRUE(IsArcPlayStoreEnabledForProfile(profile()));
  SetArcPlayStoreEnabledForProfile(profile(), false);
  EXPECT_TRUE(IsArcPlayStoreEnabledPreferenceManagedForProfile(profile()));
  EXPECT_TRUE(IsArcPlayStoreEnabledForProfile(profile()));

  // Remove managed state.
  profile()->GetTestingPrefService()->RemoveManagedPref(prefs::kArcEnabled);
  EXPECT_FALSE(IsArcPlayStoreEnabledPreferenceManagedForProfile(profile()));

  // 2) Set managed preference to false, then try to set the value to true
  // via SetArcPlayStoreEnabledForProfile().
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kArcEnabled, std::make_unique<base::Value>(false));
  EXPECT_TRUE(IsArcPlayStoreEnabledPreferenceManagedForProfile(profile()));
  EXPECT_FALSE(IsArcPlayStoreEnabledForProfile(profile()));
  SetArcPlayStoreEnabledForProfile(profile(), true);
  EXPECT_TRUE(IsArcPlayStoreEnabledPreferenceManagedForProfile(profile()));
  EXPECT_FALSE(IsArcPlayStoreEnabledForProfile(profile()));

  // Remove managed state.
  profile()->GetTestingPrefService()->RemoveManagedPref(prefs::kArcEnabled);
  EXPECT_FALSE(IsArcPlayStoreEnabledPreferenceManagedForProfile(profile()));
}

// Test the AreArcAllOptInPreferencesIgnorableForProfile() legacy behavior (Pre
// Privacy Hub).
TEST_F(ChromeArcUtilTest, AreArcAllOptInPreferencesIgnorableForProfile) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(ash::features::kCrosPrivacyHub);

  base::CommandLine::ForCurrentProcess()->InitFromArgv(
      {"", "--arc-availability=officially-supported"});
  // OptIn prefs are unset, the function returns false.
  EXPECT_FALSE(AreArcAllOptInPreferencesIgnorableForProfile(profile()));

  // OptIn prefs are set to unmanaged/OFF values, and the function returns
  // false.
  profile()->GetPrefs()->SetBoolean(prefs::kArcBackupRestoreEnabled, false);
  profile()->GetPrefs()->SetBoolean(prefs::kArcLocationServiceEnabled, false);
  EXPECT_FALSE(AreArcAllOptInPreferencesIgnorableForProfile(profile()));

  // OptIn prefs are set to unmanaged/ON values, and the function returns false.
  profile()->GetPrefs()->SetBoolean(prefs::kArcBackupRestoreEnabled, true);
  profile()->GetPrefs()->SetBoolean(prefs::kArcLocationServiceEnabled, true);
  EXPECT_FALSE(AreArcAllOptInPreferencesIgnorableForProfile(profile()));

  // Backup-restore pref is managed/OFF, while location-service is unmanaged,
  // and the function returns false.
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kArcBackupRestoreEnabled, std::make_unique<base::Value>(false));
  profile()->GetPrefs()->SetBoolean(prefs::kArcLocationServiceEnabled, false);
  // When PrivacyHubLocation is enabled, the location setting is no longer
  // ARC++ specific, but ChromeOS system setting. Thus we only check the
  // Backup-restore pref.
  EXPECT_EQ(ash::features::IsCrosPrivacyHubLocationEnabled(),
            AreArcAllOptInPreferencesIgnorableForProfile(profile()));

  // Location-service pref is managed/OFF, while backup-restore is unmanaged,
  // and the function returns false.
  profile()->GetTestingPrefService()->RemoveManagedPref(
      prefs::kArcBackupRestoreEnabled);
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kArcLocationServiceEnabled, std::make_unique<base::Value>(false));
  EXPECT_FALSE(AreArcAllOptInPreferencesIgnorableForProfile(profile()));

  // Both OptIn prefs are set to managed/OFF values, and the function returns
  // true.
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kArcBackupRestoreEnabled, std::make_unique<base::Value>(false));
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kArcLocationServiceEnabled, std::make_unique<base::Value>(false));
  EXPECT_TRUE(AreArcAllOptInPreferencesIgnorableForProfile(profile()));

  // Backup-restore pref is set to managed/ON, while location-service pref is
  // set to managed/OFF, and the function returns true.
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kArcBackupRestoreEnabled, std::make_unique<base::Value>(true));
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kArcLocationServiceEnabled, std::make_unique<base::Value>(false));
  EXPECT_TRUE(AreArcAllOptInPreferencesIgnorableForProfile(profile()));

  // Location-service pref is set to managed/ON, while location-service pref is
  // set to managed/OFF, and the function returns true.
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kArcBackupRestoreEnabled, std::make_unique<base::Value>(false));
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kArcLocationServiceEnabled, std::make_unique<base::Value>(true));
  EXPECT_TRUE(AreArcAllOptInPreferencesIgnorableForProfile(profile()));

  // Both OptIn prefs are set to managed/ON values, and the function returns
  // true.
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kArcBackupRestoreEnabled, std::make_unique<base::Value>(true));
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kArcLocationServiceEnabled, std::make_unique<base::Value>(true));
  EXPECT_TRUE(AreArcAllOptInPreferencesIgnorableForProfile(profile()));
}

// PrivacyHub introduces a system-wide location setting that Android should
// respect. In essence, this new preference, `kUserGeolocationAccessLevel`,
// replaces the functionality of `kArcLocationServiceEnabled`. Therefore, this
// utility function should check if `kUserGeolocationAccessLevel` has been set
// by policy.
TEST_F(ChromeArcUtilTest,
       AreArcAllOptInPreferencesIgnorableForProfile_PrivacyHubEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(ash::features::kCrosPrivacyHub);

  // In the legacy flow (without Privacy Hub), having both
  // `kArcBackupRestoreEnabled` and `kArcLocationServiceEnabled` managed would
  // make the function return true. With Privacy Hub enabled, it now requires
  // `kUserGeolocationAccessLevel` instead of `kArcLocationServiceEnabled` to be
  // managed; hence return false.
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kArcLocationServiceEnabled, std::make_unique<base::Value>(false));
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kArcBackupRestoreEnabled, std::make_unique<base::Value>(false));
  EXPECT_FALSE(AreArcAllOptInPreferencesIgnorableForProfile(profile()));

  // Remove `kArcLocationServiceEnabled` pref and set
  // `kUserGeolocationAccessLevel` as managed. Now the function should return
  // true.
  profile()->GetTestingPrefService()->RemoveManagedPref(
      prefs::kArcLocationServiceEnabled);
  profile()->GetTestingPrefService()->SetManagedPref(
      ash::prefs::kUserGeolocationAccessLevel,
      std::make_unique<base::Value>(1));
  EXPECT_TRUE(AreArcAllOptInPreferencesIgnorableForProfile(profile()));

  // Cleanup
  profile()->GetTestingPrefService()->RemoveManagedPref(
      prefs::kArcBackupRestoreEnabled);
  profile()->GetTestingPrefService()->RemoveManagedPref(
      ash::prefs::kUserGeolocationAccessLevel);
}

TEST_F(ChromeArcUtilTest, TermsOfServiceNegotiationNeededForAlreadyAccepted) {
  // TODO(hidehiko): Fix profile and user login creation order.
  base::CommandLine::ForCurrentProcess()->InitFromArgv(
      {"", "--arc-availability=officially-supported"});
  DisableDBusForProfileManager();
  const auto account_id = AccountId::FromUserEmailGaiaId(
      profile()->GetProfileUserName(), kTestGaiaId);
  CHECK(test_user_session_manager()->AddRegularUser(account_id));
  test_user_session_manager()->LogIn(account_id);
  EXPECT_TRUE(IsArcTermsOfServiceNegotiationNeeded(profile()));
  EXPECT_TRUE(IsArcTermsOfServiceOobeNegotiationNeeded());
  profile()->GetPrefs()->SetBoolean(prefs::kArcTermsAccepted, true);
  EXPECT_FALSE(IsArcTermsOfServiceNegotiationNeeded(profile()));
  EXPECT_FALSE(IsArcTermsOfServiceOobeNegotiationNeeded());
}

// For managed user, generally no opt-in dialog is shown.
// For OOBE user, see TermsOfServiceOobeNegotiationNeededForManagedUser test.
TEST_F(ChromeArcUtilTest, TermsOfServiceNegotiationNeededForManagedUser) {
  // TODO(hidehiko): Fix profile and user login creation order.
  base::CommandLine::ForCurrentProcess()->InitFromArgv(
      {"", "--arc-availability=officially-supported"});
  DisableDBusForProfileManager();
  const auto account_id = AccountId::FromUserEmailGaiaId(
      profile()->GetProfileUserName(), kTestGaiaId);
  CHECK(test_user_session_manager()->AddRegularUser(account_id));
  test_user_session_manager()->LogIn(account_id);

  EXPECT_TRUE(IsArcTermsOfServiceNegotiationNeeded(profile()));

  SetProfileIsManagedForTesting(profile());

  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kArcEnabled, std::make_unique<base::Value>(true));
  EXPECT_TRUE(ShouldStartArcSilentlyForManagedProfile(profile()));
  EXPECT_FALSE(IsArcTermsOfServiceNegotiationNeeded(profile()));
}

TEST_F(ChromeArcUtilTest, TermsOfServiceOobeNegotiationNeededNoLogin) {
  DisableDBusForProfileManager();
  EXPECT_FALSE(IsArcTermsOfServiceOobeNegotiationNeeded());
}

TEST_F(ChromeArcUtilTest,
       TermsOfServiceOobeNegotiationNeededNoArcAvailability) {
  // TODO(hidehiko): Fix profile and user login creation order.
  DisableDBusForProfileManager();
  const auto account_id = AccountId::FromUserEmailGaiaId(
      profile()->GetProfileUserName(), kTestGaiaId);
  CHECK(test_user_session_manager()->AddRegularUser(account_id));
  test_user_session_manager()->LogIn(account_id);
  EXPECT_FALSE(IsArcTermsOfServiceOobeNegotiationNeeded());
}

TEST_F(ChromeArcUtilTest, TermsOfServiceOobeNegotiationNeededNoPlayStore) {
  // TODO(hidehiko): Fix profile and user login creation order.
  base::CommandLine::ForCurrentProcess()->InitFromArgv(
      {"", "--arc-availability=officially-supported",
       "--arc-start-mode=always-start-with-no-play-store"});
  DisableDBusForProfileManager();
  const auto account_id = AccountId::FromUserEmailGaiaId(
      profile()->GetProfileUserName(), kTestGaiaId);
  CHECK(test_user_session_manager()->AddRegularUser(account_id));
  test_user_session_manager()->LogIn(account_id);
  EXPECT_FALSE(IsArcTermsOfServiceOobeNegotiationNeeded());
}

TEST_F(ChromeArcUtilTest, IsArcStatsReportingEnabled) {
  // TODO(hidehiko): Fix profile and user login creation order.
  base::CommandLine::ForCurrentProcess()->InitFromArgv(
      {"", "--arc-availability=officially-supported"});
  const auto account_id = AccountId::FromUserEmailGaiaId(
      profile()->GetProfileUserName(), kTestGaiaId);
  CHECK(test_user_session_manager()->AddRegularUser(account_id));
  test_user_session_manager()->LogIn(account_id);
  EXPECT_FALSE(IsArcStatsReportingEnabled());
}

TEST_F(ChromeArcUtilTest, IsArcStatsReportingEnabled_PublicAccount) {
  // TODO(hidehiko): Fix profile and user login creation order.
  base::CommandLine::ForCurrentProcess()->InitFromArgv(
      {"", "--arc-availability=officially-supported"});
  policy::DeviceLocalAccount device_local_account(
      policy::DeviceLocalAccountType::kPublicSession,
      policy::DeviceLocalAccount::EphemeralMode::kFollowDeviceWidePolicy,
      "public_user@gmail.com", /*kiosk_app_id=*/"",
      /*kiosk_app_update_url=*/"");
  CHECK(test_user_session_manager()->AddPublicAccountUser(
      device_local_account.user_id));
  test_user_session_manager()->LogIn(
      AccountId::FromUserEmail(device_local_account.user_id));
  EXPECT_FALSE(IsArcStatsReportingEnabled());
}

TEST_F(ChromeArcUtilTest, ArcStartModeDefault) {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  command_line->InitFromArgv({"", "--arc-availability=installed"});
  EXPECT_TRUE(IsPlayStoreAvailable());
}

TEST_F(ChromeArcUtilTest, ArcStartModeDefaultPublicSession) {
  // TODO(hidehiko): Fix profile and user login creation order.
  auto* command_line = base::CommandLine::ForCurrentProcess();
  command_line->InitFromArgv({"", "--arc-availability=installed"});
  policy::DeviceLocalAccount device_local_account(
      policy::DeviceLocalAccountType::kPublicSession,
      policy::DeviceLocalAccount::EphemeralMode::kFollowDeviceWidePolicy,
      "public_user@gmail.com", /*kiosk_app_id=*/"",
      /*kiosk_app_update_url=*/"");
  CHECK(test_user_session_manager()->AddPublicAccountUser(
      device_local_account.user_id));
  test_user_session_manager()->LogIn(
      AccountId::FromUserEmail(device_local_account.user_id));
  EXPECT_FALSE(IsPlayStoreAvailable());
}

TEST_F(ChromeArcUtilTest, ArcStartModeDefaultDemoMode) {
  // TODO(hidehiko): Fix profile and user login creation order.
  auto* command_line = base::CommandLine::ForCurrentProcess();
  command_line->InitFromArgv({"", "--arc-availability=installed"});
  cros_settings_test_helper_.InstallAttributes()->SetDemoMode();
  policy::DeviceLocalAccount device_local_account(
      policy::DeviceLocalAccountType::kPublicSession,
      policy::DeviceLocalAccount::EphemeralMode::kFollowDeviceWidePolicy,
      "public_user@gmail.com", /*kiosk_app_id=*/"",
      /*kiosk_app_update_url=*/"");
  CHECK(test_user_session_manager()->AddPublicAccountUser(
      device_local_account.user_id));
  test_user_session_manager()->LogIn(
      AccountId::FromUserEmail(device_local_account.user_id));
  EXPECT_TRUE(IsPlayStoreAvailable());
}

TEST_F(ChromeArcUtilTest, ArcStartModeWithoutPlayStore) {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  command_line->InitFromArgv(
      {"", "--arc-availability=installed",
       "--arc-start-mode=always-start-with-no-play-store"});
  EXPECT_FALSE(IsPlayStoreAvailable());
}

TEST_F(ChromeArcUtilTest, ArcUnmanagedToManagedTransition) {
  profile()->GetPrefs()->SetInteger(
      arc::prefs::kArcManagementTransition,
      static_cast<int>(arc::ArcManagementTransition::UNMANAGED_TO_MANAGED));

  EXPECT_EQ(GetManagementTransition(profile()),
            arc::ArcManagementTransition::UNMANAGED_TO_MANAGED);
}

class ArcOobeTest : public ChromeArcUtilTest,
                    public testing::WithParamInterface<bool> {
 public:
  ArcOobeTest() = default;
  ArcOobeTest(const ArcOobeTest&) = delete;
  ArcOobeTest& operator=(const ArcOobeTest&) = delete;
  ~ArcOobeTest() override = default;

  void SetUp() override {
    if (GetParam()) {
      scoped_feature_list_.InitAndEnableFeature(ash::features::kCrosPrivacyHub);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          ash::features::kCrosPrivacyHub);
    }
    ash::ConciergeClient::InitializeFake(/*fake_cicerone_client=*/nullptr);
    oobe_configuration_ = std::make_unique<ash::OobeConfiguration>();

    TestingBrowserProcess::GetGlobal()
        ->platform_part()
        ->InitializeComponentManager();
    TestingBrowserProcess::GetGlobal()->SetSharedURLLoaderFactory(
        test_url_loader_factory_.GetSafeWeakWrapper());

    ChromeArcUtilTest::SetUp();
  }

  void TearDown() override {
    // Fake display host have to be shut down first, as it may access
    // configuration.
    fake_login_display_host_.reset();

    ChromeArcUtilTest::TearDown();

    TestingBrowserProcess::GetGlobal()->SetSharedURLLoaderFactory(nullptr);
    TestingBrowserProcess::GetGlobal()
        ->platform_part()
        ->ShutdownComponentManager();

    oobe_configuration_.reset();
    ash::ConciergeClient::Shutdown();
  }

 protected:
  void CreateLoginDisplayHost() {
    fake_login_display_host_ = std::make_unique<ash::FakeLoginDisplayHost>();
  }

  ash::FakeLoginDisplayHost* login_display_host() {
    return fake_login_display_host_.get();
  }

  void CloseLoginDisplayHost() { fake_login_display_host_.reset(); }

 private:
  std::unique_ptr<ash::OobeConfiguration> oobe_configuration_;
  std::unique_ptr<ash::FakeLoginDisplayHost> fake_login_display_host_;
  base::test::ScopedFeatureList scoped_feature_list_;
  network::TestURLLoaderFactory test_url_loader_factory_;
};

// Testing both states of the `ash::features::kCrosPrivacyHub` feature.
INSTANTIATE_TEST_SUITE_P(All, ArcOobeTest, testing::Bool());

TEST_P(ArcOobeTest, TermsOfServiceOobeNegotiationNeededForManagedUser) {
  // TODO(hidehiko): Fix profile and user login creation order.
  base::CommandLine::ForCurrentProcess()->InitFromArgv(
      {"", "--arc-availability=officially-supported"});
  DisableDBusForProfileManager();
  const auto account_id = AccountId::FromUserEmailGaiaId(
      profile()->GetProfileUserName(), kTestGaiaId);
  CHECK(test_user_session_manager()->AddRegularUser(account_id));
  test_user_session_manager()->LogIn(account_id);

  CreateLoginDisplayHost();
  EXPECT_TRUE(IsArcOobeOptInActive());

  SetProfileIsManagedForTesting(profile());
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kArcEnabled, std::make_unique<base::Value>(true));
  EXPECT_TRUE(IsArcTermsOfServiceNegotiationNeeded(profile()));
  EXPECT_TRUE(IsArcTermsOfServiceOobeNegotiationNeeded());

  // Set `kArcBackupRestoreEnabled`as managed, this is not sufficient to skip
  // the negotiation, the location preference has to be set too.
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kArcBackupRestoreEnabled, std::make_unique<base::Value>(false));
  EXPECT_TRUE(IsArcTermsOfServiceOobeNegotiationNeeded());
  EXPECT_TRUE(IsArcTermsOfServiceNegotiationNeeded(profile()));

  if (ash::features::IsCrosPrivacyHubLocationEnabled()) {
    // When Privacy Hub is enabled, location setting is controlled by
    // `kUserGeolocationAccessLevel`.
    profile()->GetTestingPrefService()->SetManagedPref(
        ash::prefs::kUserGeolocationAccessLevel,
        std::make_unique<base::Value>(1));
    EXPECT_FALSE(IsArcTermsOfServiceOobeNegotiationNeeded());
    EXPECT_FALSE(IsArcTermsOfServiceNegotiationNeeded(profile()));
  } else {
    // When Privacy Hub is disabled, location setting is controlled by
    // `kArcLocationServiceEnabled`.
    profile()->GetTestingPrefService()->SetManagedPref(
        prefs::kArcLocationServiceEnabled,
        std::make_unique<base::Value>(false));
    EXPECT_FALSE(IsArcTermsOfServiceOobeNegotiationNeeded());
    EXPECT_FALSE(IsArcTermsOfServiceNegotiationNeeded(profile()));
  }
}

TEST_P(ArcOobeTest, ShouldStartArcSilentlyForManagedProfile) {
  // TODO(hidehiko): Fix profile and user login creation order.
  base::CommandLine::ForCurrentProcess()->InitFromArgv(
      {"", "--arc-availability=officially-supported"});
  DisableDBusForProfileManager();
  const auto account_id = AccountId::FromUserEmailGaiaId(
      profile()->GetProfileUserName(), kTestGaiaId);
  CHECK(test_user_session_manager()->AddRegularUser(account_id));
  test_user_session_manager()->LogIn(account_id);

  CreateLoginDisplayHost();
  EXPECT_TRUE(IsArcOobeOptInActive());

  SetProfileIsManagedForTesting(profile());
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kArcEnabled, std::make_unique<base::Value>(true));
  EXPECT_FALSE(ShouldStartArcSilentlyForManagedProfile(profile()));

  // Set `kArcBackupRestoreEnabled`as managed. ARC++ should not start silently,
  // location preference has to be set first.
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kArcBackupRestoreEnabled, std::make_unique<base::Value>(false));
  EXPECT_FALSE(ShouldStartArcSilentlyForManagedProfile(profile()));

  if (ash::features::IsCrosPrivacyHubLocationEnabled()) {
    // When Privacy Hub is enabled, location setting is controlled by
    // `kUserGeolocationAccessLevel`.
    profile()->GetTestingPrefService()->SetManagedPref(
        ash::prefs::kUserGeolocationAccessLevel,
        std::make_unique<base::Value>(1));
    EXPECT_TRUE(ShouldStartArcSilentlyForManagedProfile(profile()));
  } else {
    // When Privacy Hub is disabled, location setting is controlled by
    // `kArcLocationServiceEnabled`.
    profile()->GetTestingPrefService()->SetManagedPref(
        prefs::kArcLocationServiceEnabled,
        std::make_unique<base::Value>(false));
    EXPECT_TRUE(ShouldStartArcSilentlyForManagedProfile(profile()));
  }
}

using ArcOobeOptInActiveInTest = ArcOobeTest;
INSTANTIATE_TEST_SUITE_P(All, ArcOobeOptInActiveInTest, testing::Bool());

TEST_P(ArcOobeOptInActiveInTest, OobeOptInActive) {
  // TODO(hidehiko): Fix profile and user login creation order.
  // OOBE OptIn is active in case of OOBE controller is alive and the
  // Consolidated Consent screen is currently showing.
  const AccountId account_id = AccountId::FromUserEmailGaiaId(
      profile()->GetProfileUserName(), kTestGaiaId);
  CHECK(test_user_session_manager()->AddRegularUser(account_id));
  test_user_session_manager()->LogIn(account_id);
  EXPECT_FALSE(IsArcOobeOptInActive());
  CreateLoginDisplayHost();

  // OOBE OptIn can only start if Onboarding is not completed yet.
  EXPECT_TRUE(IsArcOobeOptInActive());

  // Set a version for the Onboarding to indicate that the user completed the
  // onboarding flow.
  user_manager::KnownUser(g_browser_process->local_state())
      .SetOnboardingCompletedVersion(account_id, version_info::GetVersion());
  EXPECT_FALSE(IsArcOobeOptInActive());

  // Consolidated Consent wizard but Onboarding flow completed.
  login_display_host()->StartWizard(
      ash::ConsolidatedConsentScreenView::kScreenId);
  EXPECT_FALSE(IsArcOobeOptInActive());
}

using DemoSetupFlowArcOptInTest = ArcOobeTest;
INSTANTIATE_TEST_SUITE_P(All, DemoSetupFlowArcOptInTest, testing::Bool());

TEST_P(DemoSetupFlowArcOptInTest, NoTermsOfServiceOobeNegotiationNeeded) {
  base::CommandLine::ForCurrentProcess()->InitFromArgv(
      {"", "--arc-availability=officially-supported"});
  DisableDBusForProfileManager();
  CreateLoginDisplayHost();
  EXPECT_FALSE(IsArcDemoModeSetupFlow());
  EXPECT_FALSE(IsArcTermsOfServiceOobeNegotiationNeeded());
}

TEST_P(DemoSetupFlowArcOptInTest, TermsOfServiceOobeNegotiationNeeded) {
  base::CommandLine::ForCurrentProcess()->InitFromArgv(
      {"", "--arc-availability=officially-supported"});
  DisableDBusForProfileManager();
  CreateLoginDisplayHost();
  login_display_host()->StartWizard(ash::DemoPreferencesScreenView::kScreenId);
  login_display_host()
      ->GetWizardController()
      ->SimulateDemoModeSetupForTesting();
  EXPECT_TRUE(IsArcDemoModeSetupFlow());
  EXPECT_TRUE(IsArcTermsOfServiceOobeNegotiationNeeded());
}

TEST_P(DemoSetupFlowArcOptInTest,
       NoPlayStoreNoTermsOfServiceOobeNegotiationNeeded) {
  base::CommandLine::ForCurrentProcess()->InitFromArgv(
      {"", "--arc-availability=officially-supported",
       "--arc-start-mode=always-start-with-no-play-store"});
  DisableDBusForProfileManager();
  CreateLoginDisplayHost();
  login_display_host()->StartWizard(ash::DemoPreferencesScreenView::kScreenId);
  login_display_host()
      ->GetWizardController()
      ->SimulateDemoModeSetupForTesting();
  EXPECT_TRUE(IsArcDemoModeSetupFlow());
  EXPECT_FALSE(IsArcTermsOfServiceOobeNegotiationNeeded());
}

using ChromeUnaffiliatedDevicesArcRestrictionTest = ChromeArcUtilTest;

TEST_F(ChromeUnaffiliatedDevicesArcRestrictionTest,
       ArcAllowedForAffiliatedUser_WhenPolicyValueTrue) {
  // TODO(hidehiko): Fix profile and user login creation order.
  base::CommandLine::ForCurrentProcess()->InitFromArgv(
      {"", "--arc-availability=officially-supported"});
  const auto account_id = AccountId::FromUserEmailGaiaId(
      profile()->GetProfileUserName(), kTestGaiaId);
  CHECK(test_user_session_manager()->AddRegularUser(account_id));
  test_user_session_manager()->LogIn(account_id);
  user_manager::UserManager::Get()->SetUserPolicyStatus(
      account_id, /*is_managed=*/true, /*is_affiliated=*/true);
  SetProfileIsManagedForTesting(profile());
  profile()->GetPrefs()->SetBoolean(prefs::kUnaffiliatedDeviceArcAllowed, true);

  EXPECT_TRUE(IsArcAllowedForProfileOnFirstCall(profile()));
}

TEST_F(ChromeUnaffiliatedDevicesArcRestrictionTest,
       ArcAllowedForUnAffiliatedUser_WhenPolicyValueTrue) {
  // TODO(hidehiko): Fix profile and user login creation order.
  base::CommandLine::ForCurrentProcess()->InitFromArgv(
      {"", "--arc-availability=officially-supported"});
  const auto account_id = AccountId::FromUserEmailGaiaId(
      profile()->GetProfileUserName(), kTestGaiaId);
  CHECK(test_user_session_manager()->AddRegularUser(account_id));
  test_user_session_manager()->LogIn(account_id);
  SetProfileIsManagedForTesting(profile());
  profile()->GetPrefs()->SetBoolean(prefs::kUnaffiliatedDeviceArcAllowed, true);

  EXPECT_TRUE(IsArcAllowedForProfileOnFirstCall(profile()));
}

TEST_F(ChromeUnaffiliatedDevicesArcRestrictionTest,
       ArcAllowedForNonEnterpriseAccount_WhenPolicyValueTrue) {
  // TODO(hidehiko): Fix profile and user login creation order.
  base::CommandLine::ForCurrentProcess()->InitFromArgv(
      {"", "--arc-availability=officially-supported"});
  const auto account_id = AccountId::FromUserEmailGaiaId(
      profile()->GetProfileUserName(), kTestGaiaId);
  CHECK(test_user_session_manager()->AddRegularUser(account_id));
  test_user_session_manager()->LogIn(account_id);
  profile()->GetPrefs()->SetBoolean(prefs::kUnaffiliatedDeviceArcAllowed, true);

  EXPECT_TRUE(IsArcAllowedForProfileOnFirstCall(profile()));
}

TEST_F(ChromeUnaffiliatedDevicesArcRestrictionTest,
       ArcAllowedForAffiliatedUser_WhenPolicyValueFalse) {
  // TODO(hidehiko): Fix profile and user login creation order.
  base::CommandLine::ForCurrentProcess()->InitFromArgv(
      {"", "--arc-availability=officially-supported"});
  const auto account_id = AccountId::FromUserEmailGaiaId(
      profile()->GetProfileUserName(), kTestGaiaId);
  CHECK(test_user_session_manager()->AddRegularUser(account_id));
  test_user_session_manager()->LogIn(account_id);
  user_manager::UserManager::Get()->SetUserPolicyStatus(
      account_id, /*is_managed=*/true, /*is_affiliated=*/true);
  SetProfileIsManagedForTesting(profile());
  profile()->GetPrefs()->SetBoolean(prefs::kUnaffiliatedDeviceArcAllowed,
                                    false);

  EXPECT_TRUE(IsArcAllowedForProfileOnFirstCall(profile()));
}

TEST_F(ChromeUnaffiliatedDevicesArcRestrictionTest,
       ArcNotAllowedForUnAffiliatedUser_WhenPolicyValueFalse) {
  // TODO(hidehiko): Fix profile and user login creation order.
  base::CommandLine::ForCurrentProcess()->InitFromArgv(
      {"", "--arc-availability=officially-supported"});
  const auto account_id = AccountId::FromUserEmailGaiaId(
      profile()->GetProfileUserName(), kTestGaiaId);
  CHECK(test_user_session_manager()->AddRegularUser(account_id));
  test_user_session_manager()->LogIn(account_id);
  SetProfileIsManagedForTesting(profile());
  profile()->GetPrefs()->SetBoolean(prefs::kUnaffiliatedDeviceArcAllowed,
                                    false);

  EXPECT_FALSE(IsArcAllowedForProfileOnFirstCall(profile()));
}

TEST_F(ChromeUnaffiliatedDevicesArcRestrictionTest,
       ArcAllowedForNonEnterpriseAccount_WhenPolicyValueFalse) {
  // TODO(hidehiko): Fix profile and user login creation order.
  base::CommandLine::ForCurrentProcess()->InitFromArgv(
      {"", "--arc-availability=officially-supported"});
  const auto account_id = AccountId::FromUserEmailGaiaId(
      profile()->GetProfileUserName(), kTestGaiaId);
  CHECK(test_user_session_manager()->AddRegularUser(account_id));
  test_user_session_manager()->LogIn(account_id);
  profile()->GetPrefs()->SetBoolean(prefs::kUnaffiliatedDeviceArcAllowed,
                                    false);

  EXPECT_TRUE(IsArcAllowedForProfileOnFirstCall(profile()));
}

TEST_F(ChromeUnaffiliatedDevicesArcRestrictionTest,
       ReportArcAllowedForAffiliatedUser_WhenPolicyValueFalse) {
  // TODO(hidehiko): Fix profile and user login creation order.
  base::HistogramTester tester;
  base::CommandLine::ForCurrentProcess()->InitFromArgv(
      {"", "--arc-availability=officially-supported"});
  const auto account_id = AccountId::FromUserEmailGaiaId(
      profile()->GetProfileUserName(), kTestGaiaId);
  CHECK(test_user_session_manager()->AddRegularUser(account_id));
  test_user_session_manager()->LogIn(account_id);
  user_manager::UserManager::Get()->SetUserPolicyStatus(
      account_id, /*is_managed=*/true, /*is_affiliated=*/true);
  SetProfileIsManagedForTesting(profile());
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kArcEnabled, std::make_unique<base::Value>(true));
  profile()->GetPrefs()->SetBoolean(prefs::kUnaffiliatedDeviceArcAllowed,
                                    false);

  RecordArcStatusBasedOnDeviceAffiliationUMA(profile());
  tester.ExpectBucketCount("Arc.Provisioning.DeviceAffiliationAction", 0, 1);
}

TEST_F(ChromeUnaffiliatedDevicesArcRestrictionTest,
       ReportArcAllowedForUnAffiliatedUser_WhenPolicyValueTrue) {
  // TODO(hidehiko): Fix profile and user login creation order.
  base::HistogramTester tester;
  base::CommandLine::ForCurrentProcess()->InitFromArgv(
      {"", "--arc-availability=officially-supported"});
  const auto account_id = AccountId::FromUserEmailGaiaId(
      profile()->GetProfileUserName(), kTestGaiaId);
  CHECK(test_user_session_manager()->AddRegularUser(account_id));
  test_user_session_manager()->LogIn(account_id);
  SetProfileIsManagedForTesting(profile());
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kArcEnabled, std::make_unique<base::Value>(true));
  profile()->GetPrefs()->SetBoolean(prefs::kUnaffiliatedDeviceArcAllowed, true);
  RecordArcStatusBasedOnDeviceAffiliationUMA(profile());
  tester.ExpectBucketCount("Arc.Provisioning.DeviceAffiliationAction", 1, 1);
}

TEST_F(ChromeUnaffiliatedDevicesArcRestrictionTest,
       ReportArcNotAllowedForUnAffiliatedUser_WhenPolicyValueFalse) {
  // TODO(hidehiko): Fix profile and user login creation order.
  base::HistogramTester tester;
  base::CommandLine::ForCurrentProcess()->InitFromArgv(
      {"", "--arc-availability=officially-supported"});
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kArcEnabled, std::make_unique<base::Value>(true));
  const auto account_id = AccountId::FromUserEmailGaiaId(
      profile()->GetProfileUserName(), kTestGaiaId);
  CHECK(test_user_session_manager()->AddRegularUser(account_id));
  test_user_session_manager()->LogIn(account_id);
  SetProfileIsManagedForTesting(profile());
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kArcEnabled, std::make_unique<base::Value>(true));
  profile()->GetPrefs()->SetBoolean(prefs::kUnaffiliatedDeviceArcAllowed,
                                    false);
  RecordArcStatusBasedOnDeviceAffiliationUMA(profile());
  tester.ExpectBucketCount("Arc.Provisioning.DeviceAffiliationAction", 2, 1);
}

}  // namespace util
}  // namespace arc
