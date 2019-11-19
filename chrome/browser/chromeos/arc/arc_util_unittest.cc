// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/arc_util.h"

#include <memory>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/system/sys_info.h"
#include "base/test/icu_test_util.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/arc/session/arc_session_manager.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_session.h"
#include "chrome/browser/chromeos/login/oobe_configuration.h"
#include "chrome/browser/chromeos/login/ui/fake_login_display_host.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chromeos/login/demo_preferences_screen_handler.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_oobe_configuration_client.h"
#include "chromeos/tpm/stub_install_attributes.h"
#include "components/account_id/account_id.h"
#include "components/arc/arc_prefs.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_names.h"
#include "content/public/browser/notification_service.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {
namespace util {

namespace {

constexpr char kTestProfileName[] = "user@gmail.com";
constexpr char kTestGaiaId[] = "1234567890";

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

class FakeUserManagerWithLocalState : public chromeos::FakeChromeUserManager {
 public:
  explicit FakeUserManagerWithLocalState(
      TestingProfileManager* testing_profile_manager)
      : testing_profile_manager_(testing_profile_manager),
        test_local_state_(std::make_unique<TestingPrefServiceSimple>()) {
    RegisterPrefs(test_local_state_->registry());
  }

  PrefService* GetLocalState() const override {
    return test_local_state_.get();
  }

  TestingProfileManager* testing_profile_manager() {
    return testing_profile_manager_;
  }

 private:
  // Unowned pointer.
  TestingProfileManager* const testing_profile_manager_;

  std::unique_ptr<TestingPrefServiceSimple> test_local_state_;

  DISALLOW_COPY_AND_ASSIGN(FakeUserManagerWithLocalState);
};

class ScopedLogIn {
 public:
  ScopedLogIn(
      FakeUserManagerWithLocalState* fake_user_manager,
      const AccountId& account_id,
      user_manager::UserType user_type = user_manager::USER_TYPE_REGULAR)
      : fake_user_manager_(fake_user_manager), account_id_(account_id) {
    // Prevent access to DBus. This switch is reset in case set from test SetUp
    // due massive usage of InitFromArgv.
    base::CommandLine& command_line = *base::CommandLine::ForCurrentProcess();
    if (!command_line.HasSwitch(switches::kTestType))
      command_line.AppendSwitch(switches::kTestType);

    switch (user_type) {
      case user_manager::USER_TYPE_REGULAR:  // fallthrough
      case user_manager::USER_TYPE_ACTIVE_DIRECTORY:
        LogIn();
        break;
      case user_manager::USER_TYPE_PUBLIC_ACCOUNT:
        LogInAsPublicAccount();
        break;
      case user_manager::USER_TYPE_ARC_KIOSK_APP:
        LogInArcKioskApp();
        break;
      default:
        NOTREACHED();
    }
  }

  ~ScopedLogIn() { fake_user_manager_->RemoveUserFromList(account_id_); }

 private:
  void LogIn() {
    fake_user_manager_->AddUser(account_id_);
    fake_user_manager_->LoginUser(account_id_);
  }

  void LogInAsPublicAccount() {
    fake_user_manager_->AddPublicAccountUser(account_id_);
    fake_user_manager_->LoginUser(account_id_);
  }

  void LogInArcKioskApp() {
    fake_user_manager_->AddArcKioskAppUser(account_id_);
    fake_user_manager_->LoginUser(account_id_);
  }

  FakeUserManagerWithLocalState* fake_user_manager_;
  const AccountId account_id_;

  DISALLOW_COPY_AND_ASSIGN(ScopedLogIn);
};

bool IsArcAllowedForProfileOnFirstCall(const Profile* profile) {
  ResetArcAllowedCheckForTesting(profile);
  return IsArcAllowedForProfile(profile);
}

}  // namespace

class ChromeArcUtilTest : public testing::Test {
 public:
  ChromeArcUtilTest() = default;
  ~ChromeArcUtilTest() override = default;

  void SetUp() override {
    command_line_ = std::make_unique<base::test::ScopedCommandLine>();

    ASSERT_TRUE(data_dir_.CreateUniqueTempDir());
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());

    user_manager_enabler_ = std::make_unique<user_manager::ScopedUserManager>(
        std::make_unique<FakeUserManagerWithLocalState>(
            profile_manager_.get()));

    profile_ = profile_manager_->CreateTestingProfile(kTestProfileName);
  }

  void TearDown() override {
    // Avoid retries, let the next test start safely.
    ResetArcAllowedCheckForTesting(profile_);
    profile_manager_->DeleteTestingProfile(kTestProfileName);
    profile_ = nullptr;
    user_manager_enabler_.reset();
    profile_manager_.reset();
    command_line_.reset();
  }

  TestingProfile* profile() { return profile_; }

  FakeUserManagerWithLocalState* GetFakeUserManager() const {
    return static_cast<FakeUserManagerWithLocalState*>(
        user_manager::UserManager::Get());
  }

  void LogIn() {
    const auto account_id = AccountId::FromUserEmailGaiaId(
        profile()->GetProfileUserName(), kTestGaiaId);
    GetFakeUserManager()->AddUser(account_id);
    GetFakeUserManager()->LoginUser(account_id);
  }

 private:
  std::unique_ptr<base::test::ScopedCommandLine> command_line_;
  base::test::ScopedFeatureList feature_list_;
  content::BrowserTaskEnvironment task_environment_;
  chromeos::ScopedCrosSettingsTestHelper cros_settings_test_helper_;
  base::ScopedTempDir data_dir_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  std::unique_ptr<user_manager::ScopedUserManager> user_manager_enabler_;
  // Owned by |profile_manager_|
  TestingProfile* profile_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ChromeArcUtilTest);
};

TEST_F(ChromeArcUtilTest, IsArcAllowedForProfile) {
  base::CommandLine::ForCurrentProcess()->InitFromArgv(
      {"", "--arc-availability=officially-supported"});
  ScopedLogIn login(GetFakeUserManager(),
                    AccountId::FromUserEmailGaiaId(
                        profile()->GetProfileUserName(), kTestGaiaId));
  EXPECT_TRUE(IsArcAllowedForProfileOnFirstCall(profile()));

  // false for nullptr.
  EXPECT_FALSE(IsArcAllowedForProfileOnFirstCall(nullptr));

  // false for incognito mode profile.
  EXPECT_FALSE(
      IsArcAllowedForProfileOnFirstCall(profile()->GetOffTheRecordProfile()));

  // false for Legacy supervised user.
  profile()->SetSupervisedUserId("foo");
  EXPECT_FALSE(IsArcAllowedForProfileOnFirstCall(profile()));
}

TEST_F(ChromeArcUtilTest, IsArcAllowedForProfileLegacy) {
  base::CommandLine::ForCurrentProcess()->InitFromArgv({"", "--enable-arc"});
  ScopedLogIn login(GetFakeUserManager(),
                    AccountId::FromUserEmailGaiaId(
                        profile()->GetProfileUserName(), kTestGaiaId));
  EXPECT_TRUE(IsArcAllowedForProfileOnFirstCall(profile()));

  // false for nullptr.
  EXPECT_FALSE(IsArcAllowedForProfileOnFirstCall(nullptr));

  // false for incognito mode profile.
  EXPECT_FALSE(
      IsArcAllowedForProfileOnFirstCall(profile()->GetOffTheRecordProfile()));

  // false for Legacy supervised user.
  profile()->SetSupervisedUserId("foo");
  EXPECT_FALSE(IsArcAllowedForProfileOnFirstCall(profile()));
}

TEST_F(ChromeArcUtilTest, IsArcAllowedForProfile_DisableArc) {
  base::CommandLine::ForCurrentProcess()->InitFromArgv({""});
  ScopedLogIn login(GetFakeUserManager(),
                    AccountId::FromUserEmailGaiaId(
                        profile()->GetProfileUserName(), kTestGaiaId));
  EXPECT_FALSE(IsArcAllowedForProfileOnFirstCall(profile()));
}

TEST_F(ChromeArcUtilTest, IsArcAllowedForProfile_NonPrimaryProfile) {
  base::CommandLine::ForCurrentProcess()->InitFromArgv(
      {"", "--arc-availability=officially-supported"});
  ScopedLogIn login2(
      GetFakeUserManager(),
      AccountId::FromUserEmailGaiaId("user2@gmail.com", "0123456789"));
  ScopedLogIn login(GetFakeUserManager(),
                    AccountId::FromUserEmailGaiaId(
                        profile()->GetProfileUserName(), kTestGaiaId));
  EXPECT_FALSE(IsArcAllowedForProfileOnFirstCall(profile()));
}

// User without GAIA account.
TEST_F(ChromeArcUtilTest, IsArcAllowedForProfile_PublicAccount) {
  base::CommandLine::ForCurrentProcess()->InitFromArgv(
      {"", "--arc-availability=officially-supported"});
  ScopedLogIn login(GetFakeUserManager(),
                    AccountId::FromUserEmail("public_user@gmail.com"),
                    user_manager::USER_TYPE_PUBLIC_ACCOUNT);
  EXPECT_TRUE(IsArcAllowedForProfile(profile()));
}

TEST_F(ChromeArcUtilTest, IsArcAllowedForProfile_ActiveDirectoryEnabled) {
  base::CommandLine::ForCurrentProcess()->InitFromArgv(
      {"", "--arc-availability=officially-supported"});
  ScopedLogIn login(
      GetFakeUserManager(),
      AccountId::AdFromObjGuid("f04557de-5da2-40ce-ae9d-b8874d8da96e"),
      user_manager::USER_TYPE_ACTIVE_DIRECTORY);
  EXPECT_FALSE(chromeos::ProfileHelper::Get()
                   ->GetUserByProfile(profile())
                   ->HasGaiaAccount());
  EXPECT_TRUE(IsArcAllowedForProfileOnFirstCall(profile()));
}

TEST_F(ChromeArcUtilTest, IsArcAllowedForProfile_ActiveDirectoryDisabled) {
  base::CommandLine::ForCurrentProcess()->InitFromArgv({""});
  ScopedLogIn login(
      GetFakeUserManager(),
      AccountId::AdFromObjGuid("f04557de-5da2-40ce-ae9d-b8874d8da96e"),
      user_manager::USER_TYPE_ACTIVE_DIRECTORY);
  EXPECT_FALSE(chromeos::ProfileHelper::Get()
                   ->GetUserByProfile(profile())
                   ->HasGaiaAccount());
  EXPECT_FALSE(IsArcAllowedForProfileOnFirstCall(profile()));
}

TEST_F(ChromeArcUtilTest, IsArcAllowedForProfile_KioskArcNotAvailable) {
  base::CommandLine::ForCurrentProcess()->InitFromArgv({""});
  ScopedLogIn login(GetFakeUserManager(),
                    AccountId::FromUserEmail(profile()->GetProfileUserName()),
                    user_manager::USER_TYPE_ARC_KIOSK_APP);
  EXPECT_FALSE(chromeos::ProfileHelper::Get()
                   ->GetUserByProfile(profile())
                   ->HasGaiaAccount());
  EXPECT_FALSE(IsArcAllowedForProfileOnFirstCall(profile()));
}

TEST_F(ChromeArcUtilTest, IsArcAllowedForProfile_KioskArcInstalled) {
  base::CommandLine::ForCurrentProcess()->InitFromArgv(
      {"", "--arc-availability=installed"});
  ScopedLogIn login(GetFakeUserManager(),
                    AccountId::FromUserEmail(profile()->GetProfileUserName()),
                    user_manager::USER_TYPE_ARC_KIOSK_APP);
  EXPECT_FALSE(chromeos::ProfileHelper::Get()
                   ->GetUserByProfile(profile())
                   ->HasGaiaAccount());
  EXPECT_TRUE(IsArcAllowedForProfileOnFirstCall(profile()));
}

TEST_F(ChromeArcUtilTest, IsArcAllowedForProfile_KioskArcSupported) {
  base::CommandLine::ForCurrentProcess()->InitFromArgv(
      {"", "--arc-availability=officially-supported"});
  ScopedLogIn login(GetFakeUserManager(),
                    AccountId::FromUserEmail(profile()->GetProfileUserName()),
                    user_manager::USER_TYPE_ARC_KIOSK_APP);
  EXPECT_FALSE(chromeos::ProfileHelper::Get()
                   ->GetUserByProfile(profile())
                   ->HasGaiaAccount());
  EXPECT_TRUE(IsArcAllowedForProfileOnFirstCall(profile()));
}

// Guest account is interpreted as EphemeralDataUser.
TEST_F(ChromeArcUtilTest, IsArcAllowedForProfile_GuestAccount) {
  base::CommandLine::ForCurrentProcess()->InitFromArgv(
      {"", "--arc-availability=officially-supported"});
  ScopedLogIn login(GetFakeUserManager(),
                    GetFakeUserManager()->GetGuestAccountId());
  EXPECT_TRUE(IsArcAllowedForProfileOnFirstCall(profile()));
}

// Demo account is interpreted as EphemeralDataUser.
TEST_F(ChromeArcUtilTest, IsArcAllowedForProfile_DemoAccount) {
  base::CommandLine::ForCurrentProcess()->InitFromArgv(
      {"", "--arc-availability=officially-supported"});
  ScopedLogIn login(GetFakeUserManager(), user_manager::DemoAccountId());
  EXPECT_TRUE(IsArcAllowedForProfileOnFirstCall(profile()));
}

TEST_F(ChromeArcUtilTest, IsArcBlockedDueToIncompatibleFileSystem) {
  base::CommandLine::ForCurrentProcess()->InitFromArgv(
      {"", "--arc-availability=officially-supported"});
  SetArcBlockedDueToIncompatibleFileSystemForTesting(true);

  const AccountId user_id(AccountId::FromUserEmailGaiaId(
      profile()->GetProfileUserName(), kTestGaiaId));
  const AccountId robot_id(
      AccountId::FromUserEmail(profile()->GetProfileUserName()));

  // Blocked for a regular user.
  {
    ScopedLogIn login(GetFakeUserManager(), user_id,
                      user_manager::USER_TYPE_REGULAR);
    EXPECT_TRUE(IsArcBlockedDueToIncompatibleFileSystem(profile()));
  }

  // Never blocked for an ARC kiosk.
  {
    ScopedLogIn login(GetFakeUserManager(), robot_id,
                      user_manager::USER_TYPE_ARC_KIOSK_APP);
    EXPECT_FALSE(IsArcBlockedDueToIncompatibleFileSystem(profile()));
  }

  // Never blocked for a public session.
  {
    ScopedLogIn login(GetFakeUserManager(), robot_id,
                      user_manager::USER_TYPE_PUBLIC_ACCOUNT);
    EXPECT_FALSE(IsArcBlockedDueToIncompatibleFileSystem(profile()));
  }
}

TEST_F(ChromeArcUtilTest, IsArcCompatibleFileSystemUsedForProfile) {
  base::CommandLine::ForCurrentProcess()->InitFromArgv(
      {"", "--arc-availability=officially-supported"});

  const AccountId id(AccountId::FromUserEmailGaiaId(
      profile()->GetProfileUserName(), kTestGaiaId));
  ScopedLogIn login(GetFakeUserManager(), id);
  const user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(profile());

  // Unconfirmed
  EXPECT_FALSE(IsArcCompatibleFileSystemUsedForUser(user));

  // Old FS
  user_manager::known_user::SetIntegerPref(
      id, prefs::kArcCompatibleFilesystemChosen, kFileSystemIncompatible);
  EXPECT_FALSE(IsArcCompatibleFileSystemUsedForUser(user));

  // New FS
  user_manager::known_user::SetIntegerPref(
      id, prefs::kArcCompatibleFilesystemChosen, kFileSystemCompatible);
  EXPECT_TRUE(IsArcCompatibleFileSystemUsedForUser(user));

  // New FS (User notified)
  user_manager::known_user::SetIntegerPref(
      id, prefs::kArcCompatibleFilesystemChosen,
      kFileSystemCompatibleAndNotifiedDeprecated);
  EXPECT_TRUE(IsArcCompatibleFileSystemUsedForUser(user));
}

TEST_F(ChromeArcUtilTest, ArcPlayStoreEnabledForProfile) {
  base::CommandLine::ForCurrentProcess()->InitFromArgv(
      {"", "--arc-availability=officially-supported"});
  // Ensure IsAllowedForProfile() true.
  ScopedLogIn login(GetFakeUserManager(),
                    AccountId::FromUserEmailGaiaId(
                        profile()->GetProfileUserName(), kTestGaiaId));
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
  base::CommandLine::ForCurrentProcess()->InitFromArgv(
      {"", "--arc-availability=officially-supported"});
  // Ensure IsAllowedForProfile() true.
  ScopedLogIn login(GetFakeUserManager(),
                    AccountId::FromUserEmailGaiaId(
                        profile()->GetProfileUserName(), kTestGaiaId));
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

// Test the AreArcAllOptInPreferencesIgnorableForProfile() function.
TEST_F(ChromeArcUtilTest, AreArcAllOptInPreferencesIgnorableForProfile) {
  base::CommandLine::ForCurrentProcess()->InitFromArgv(
      {"", "--arc-availability=officially-supported"});
  // OptIn prefs are unset, the function returns false.
  EXPECT_FALSE(AreArcAllOptInPreferencesIgnorableForProfile(profile()));

  // Prefs are unused for Active Directory users.
  {
    ScopedLogIn login(GetFakeUserManager(),
                      AccountId::AdFromUserEmailObjGuid(
                          profile()->GetProfileUserName(), kTestGaiaId));
    EXPECT_TRUE(AreArcAllOptInPreferencesIgnorableForProfile(profile()));
  }

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
  EXPECT_FALSE(AreArcAllOptInPreferencesIgnorableForProfile(profile()));

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

// Test the IsActiveDirectoryUserForProfile() function for non-AD accounts.
TEST_F(ChromeArcUtilTest, IsActiveDirectoryUserForProfile_Gaia) {
  ScopedLogIn login(GetFakeUserManager(),
                    AccountId::FromUserEmailGaiaId(
                        profile()->GetProfileUserName(), kTestGaiaId));
  EXPECT_FALSE(IsActiveDirectoryUserForProfile(profile()));
}

// Test the IsActiveDirectoryUserForProfile() function for AD accounts.
TEST_F(ChromeArcUtilTest, IsActiveDirectoryUserForProfile_AD) {
  ScopedLogIn login(GetFakeUserManager(),
                    AccountId::AdFromUserEmailObjGuid(
                        profile()->GetProfileUserName(), kTestGaiaId));
  EXPECT_TRUE(IsActiveDirectoryUserForProfile(profile()));
}

TEST_F(ChromeArcUtilTest, TermsOfServiceNegotiationNeededForAlreadyAccepted) {
  base::CommandLine::ForCurrentProcess()->InitFromArgv(
      {"", "--arc-availability=officially-supported"});
  DisableDBusForProfileManager();
  ScopedLogIn login(GetFakeUserManager(),
                    AccountId::FromUserEmailGaiaId(
                        profile()->GetProfileUserName(), kTestGaiaId));
  EXPECT_TRUE(IsArcTermsOfServiceNegotiationNeeded(profile()));
  EXPECT_TRUE(IsArcTermsOfServiceOobeNegotiationNeeded());
  profile()->GetPrefs()->SetBoolean(prefs::kArcTermsAccepted, true);
  EXPECT_FALSE(IsArcTermsOfServiceNegotiationNeeded(profile()));
  EXPECT_FALSE(IsArcTermsOfServiceOobeNegotiationNeeded());
}

// For managed user, generally no opt-in dialog is shown.
// For OOBE user, see TermsOfServiceOobeNegotiationNeededForManagedUser test.
TEST_F(ChromeArcUtilTest, TermsOfServiceNegotiationNeededForManagedUser) {
  base::CommandLine::ForCurrentProcess()->InitFromArgv(
      {"", "--arc-availability=officially-supported"});
  DisableDBusForProfileManager();
  ScopedLogIn login(GetFakeUserManager(),
                    AccountId::FromUserEmailGaiaId(
                        profile()->GetProfileUserName(), kTestGaiaId));

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
  DisableDBusForProfileManager();
  ScopedLogIn login(GetFakeUserManager(),
                    AccountId::FromUserEmailGaiaId(
                        profile()->GetProfileUserName(), kTestGaiaId));
  EXPECT_FALSE(IsArcTermsOfServiceOobeNegotiationNeeded());
}

TEST_F(ChromeArcUtilTest, TermsOfServiceOobeNegotiationNeededNoPlayStore) {
  base::CommandLine::ForCurrentProcess()->InitFromArgv(
      {"", "--arc-availability=officially-supported",
       "--arc-start-mode=always-start-with-no-play-store"});
  DisableDBusForProfileManager();
  ScopedLogIn login(GetFakeUserManager(),
                    AccountId::FromUserEmailGaiaId(
                        profile()->GetProfileUserName(), kTestGaiaId));
  EXPECT_FALSE(IsArcTermsOfServiceOobeNegotiationNeeded());
}

TEST_F(ChromeArcUtilTest, TermsOfServiceOobeNegotiationNeededAdUser) {
  base::CommandLine::ForCurrentProcess()->InitFromArgv(
      {"", "--arc-availability=officially-supported"});
  DisableDBusForProfileManager();
  ScopedLogIn login(GetFakeUserManager(),
                    AccountId::AdFromUserEmailObjGuid(
                        profile()->GetProfileUserName(), kTestGaiaId));
  EXPECT_FALSE(IsArcTermsOfServiceOobeNegotiationNeeded());
}

TEST_F(ChromeArcUtilTest, IsArcStatsReportingEnabled) {
  base::CommandLine::ForCurrentProcess()->InitFromArgv(
      {"", "--arc-availability=officially-supported"});
  ScopedLogIn login(GetFakeUserManager(),
                    AccountId::FromUserEmailGaiaId(
                        profile()->GetProfileUserName(), kTestGaiaId));
  EXPECT_FALSE(IsArcStatsReportingEnabled());
}

TEST_F(ChromeArcUtilTest, IsArcStatsReportingEnabled_PublicAccount) {
  base::CommandLine::ForCurrentProcess()->InitFromArgv(
      {"", "--arc-availability=officially-supported"});
  ScopedLogIn login(GetFakeUserManager(),
                    AccountId::FromUserEmail("public_user@gmail.com"),
                    user_manager::USER_TYPE_PUBLIC_ACCOUNT);
  EXPECT_FALSE(IsArcStatsReportingEnabled());
}

TEST_F(ChromeArcUtilTest, ArcStartModeDefault) {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  command_line->InitFromArgv({"", "--arc-availability=installed"});
  EXPECT_TRUE(IsPlayStoreAvailable());
}

TEST_F(ChromeArcUtilTest, ArcStartModeDefaultPublicSession) {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  command_line->InitFromArgv({"", "--arc-availability=installed"});
  ScopedLogIn login(GetFakeUserManager(),
                    AccountId::FromUserEmail("public_user@gmail.com"),
                    user_manager::USER_TYPE_PUBLIC_ACCOUNT);
  EXPECT_FALSE(IsPlayStoreAvailable());
}

TEST_F(ChromeArcUtilTest, ArcStartModeDefaultDemoMode) {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  command_line->InitFromArgv({"", "--arc-availability=installed"});
  chromeos::DemoSession::SetDemoConfigForTesting(
      chromeos::DemoSession::DemoModeConfig::kOnline);
  ScopedLogIn login(GetFakeUserManager(),
                    AccountId::FromUserEmail("public_user@gmail.com"),
                    user_manager::USER_TYPE_PUBLIC_ACCOUNT);
  EXPECT_FALSE(IsPlayStoreAvailable());
}

TEST_F(ChromeArcUtilTest, ArcStartModeDefaultDemoModeWithPlayStore) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatureState(chromeos::features::kShowPlayInDemoMode,
                                    true /* enabled */);
  auto* command_line = base::CommandLine::ForCurrentProcess();
  command_line->InitFromArgv({"", "--arc-availability=installed"});
  chromeos::DemoSession::SetDemoConfigForTesting(
      chromeos::DemoSession::DemoModeConfig::kOnline);
  ScopedLogIn login(GetFakeUserManager(),
                    AccountId::FromUserEmail("public_user@gmail.com"),
                    user_manager::USER_TYPE_PUBLIC_ACCOUNT);
  EXPECT_TRUE(IsPlayStoreAvailable());
}

TEST_F(ChromeArcUtilTest, ArcStartModeWithoutPlayStore) {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  command_line->InitFromArgv(
      {"", "--arc-availability=installed",
       "--arc-start-mode=always-start-with-no-play-store"});
  EXPECT_FALSE(IsPlayStoreAvailable());
}

using ArcMigrationTest = ChromeArcUtilTest;

TEST_F(ArcMigrationTest, IsMigrationAllowedUnmanagedUser) {
  ScopedLogIn login(GetFakeUserManager(),
                    AccountId::AdFromUserEmailObjGuid(
                        profile()->GetProfileUserName(), kTestGaiaId));
  profile()->GetPrefs()->SetInteger(prefs::kEcryptfsMigrationStrategy, 0);
  EXPECT_TRUE(IsArcMigrationAllowedByPolicyForProfile(profile()));
}

TEST_F(ArcMigrationTest, IsMigrationAllowedDefaultManagedUser) {
  // Don't set any value for kEcryptfsMigrationStrategy pref.
  ScopedLogIn login(GetFakeUserManager(),
                    AccountId::FromUserEmailGaiaId(
                        profile()->GetProfileUserName(), kTestGaiaId));
  SetProfileIsManagedForTesting(profile());
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kArcEnabled, std::make_unique<base::Value>(true));
  EXPECT_FALSE(IsArcMigrationAllowedByPolicyForProfile(profile()));
}

TEST_F(ArcMigrationTest, IsMigrationAllowedForbiddenByPolicy) {
  ScopedLogIn login(GetFakeUserManager(),
                    AccountId::AdFromUserEmailObjGuid(
                        profile()->GetProfileUserName(), kTestGaiaId));
  SetProfileIsManagedForTesting(profile());
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kArcEnabled, std::make_unique<base::Value>(true));
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kEcryptfsMigrationStrategy, std::make_unique<base::Value>(0));
  EXPECT_FALSE(IsArcMigrationAllowedByPolicyForProfile(profile()));
}

TEST_F(ArcMigrationTest, IsMigrationAllowedMigrate) {
  ScopedLogIn login(GetFakeUserManager(),
                    AccountId::AdFromUserEmailObjGuid(
                        profile()->GetProfileUserName(), kTestGaiaId));
  SetProfileIsManagedForTesting(profile());
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kArcEnabled, std::make_unique<base::Value>(true));
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kEcryptfsMigrationStrategy, std::make_unique<base::Value>(1));
  EXPECT_TRUE(IsArcMigrationAllowedByPolicyForProfile(profile()));
}

TEST_F(ArcMigrationTest, IsMigrationAllowedWipe) {
  ScopedLogIn login(GetFakeUserManager(),
                    AccountId::AdFromUserEmailObjGuid(
                        profile()->GetProfileUserName(), kTestGaiaId));
  SetProfileIsManagedForTesting(profile());
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kArcEnabled, std::make_unique<base::Value>(true));
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kEcryptfsMigrationStrategy, std::make_unique<base::Value>(2));
  EXPECT_TRUE(IsArcMigrationAllowedByPolicyForProfile(profile()));
}

TEST_F(ArcMigrationTest, IsMigrationAllowedMinimalMigration) {
  ScopedLogIn login(GetFakeUserManager(),
                    AccountId::AdFromUserEmailObjGuid(
                        profile()->GetProfileUserName(), kTestGaiaId));
  SetProfileIsManagedForTesting(profile());
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kArcEnabled, std::make_unique<base::Value>(true));
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kEcryptfsMigrationStrategy, std::make_unique<base::Value>(4));
  EXPECT_TRUE(IsArcMigrationAllowedByPolicyForProfile(profile()));
}

TEST_F(ArcMigrationTest, IsMigrationAllowedCachedValueForbidden) {
  ScopedLogIn login(GetFakeUserManager(),
                    AccountId::AdFromUserEmailObjGuid(
                        profile()->GetProfileUserName(), kTestGaiaId));
  SetProfileIsManagedForTesting(profile());
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kArcEnabled, std::make_unique<base::Value>(true));
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kEcryptfsMigrationStrategy, std::make_unique<base::Value>(0));
  EXPECT_FALSE(IsArcMigrationAllowedByPolicyForProfile(profile()));
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kEcryptfsMigrationStrategy, std::make_unique<base::Value>(1));

  // The value of IsArcMigrationAllowedByPolicyForProfile() should be cached.
  // So, even if the policy is set after the first call, the returned value
  // should not be changed.
  EXPECT_FALSE(IsArcMigrationAllowedByPolicyForProfile(profile()));
}

TEST_F(ArcMigrationTest, IsMigrationAllowedCachedValueAllowed) {
  ScopedLogIn login(GetFakeUserManager(),
                    AccountId::AdFromUserEmailObjGuid(
                        profile()->GetProfileUserName(), kTestGaiaId));
  SetProfileIsManagedForTesting(profile());
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kArcEnabled, std::make_unique<base::Value>(true));
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kEcryptfsMigrationStrategy, std::make_unique<base::Value>(1));
  EXPECT_TRUE(IsArcMigrationAllowedByPolicyForProfile(profile()));
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kEcryptfsMigrationStrategy, std::make_unique<base::Value>(0));
  EXPECT_TRUE(IsArcMigrationAllowedByPolicyForProfile(profile()));
}

class ArcOobeTest : public ChromeArcUtilTest {
 public:
  ArcOobeTest() {
    chromeos::DBusThreadManager::GetSetterForTesting();
    oobe_configuration_ = std::make_unique<chromeos::OobeConfiguration>();
  }

  ~ArcOobeTest() override {
    // Fake display host have to be shut down first, as it may access
    // configuration.
    fake_login_display_host_.reset();
    oobe_configuration_.reset();
    chromeos::DBusThreadManager::Shutdown();
  }

 protected:
  void CreateLoginDisplayHost() {
    fake_login_display_host_ =
        std::make_unique<chromeos::FakeLoginDisplayHost>();
  }

  chromeos::FakeLoginDisplayHost* login_display_host() {
    return fake_login_display_host_.get();
  }

  void CloseLoginDisplayHost() { fake_login_display_host_.reset(); }

 private:
  std::unique_ptr<chromeos::OobeConfiguration> oobe_configuration_;
  std::unique_ptr<chromeos::FakeLoginDisplayHost> fake_login_display_host_;

  DISALLOW_COPY_AND_ASSIGN(ArcOobeTest);
};

TEST_F(ArcOobeTest, TermsOfServiceOobeNegotiationNeededForManagedUser) {
  base::CommandLine::ForCurrentProcess()->InitFromArgv(
      {"", "--arc-availability=officially-supported"});
  DisableDBusForProfileManager();
  ScopedLogIn login(GetFakeUserManager(),
                    AccountId::FromUserEmailGaiaId(
                        profile()->GetProfileUserName(), kTestGaiaId));

  GetFakeUserManager()->set_current_user_new(true);
  CreateLoginDisplayHost();
  EXPECT_TRUE(IsArcOobeOptInActive());

  SetProfileIsManagedForTesting(profile());
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kArcEnabled, std::make_unique<base::Value>(true));
  EXPECT_TRUE(IsArcTermsOfServiceNegotiationNeeded(profile()));
  EXPECT_TRUE(IsArcTermsOfServiceOobeNegotiationNeeded());

  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kArcBackupRestoreEnabled, std::make_unique<base::Value>(false));
  EXPECT_TRUE(IsArcTermsOfServiceNegotiationNeeded(profile()));
  EXPECT_TRUE(IsArcTermsOfServiceOobeNegotiationNeeded());

  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kArcLocationServiceEnabled, std::make_unique<base::Value>(false));
  EXPECT_FALSE(IsArcTermsOfServiceNegotiationNeeded(profile()));
  EXPECT_FALSE(IsArcTermsOfServiceOobeNegotiationNeeded());

  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kArcBackupRestoreEnabled, std::make_unique<base::Value>(true));
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kArcLocationServiceEnabled, std::make_unique<base::Value>(false));
  EXPECT_FALSE(IsArcTermsOfServiceNegotiationNeeded(profile()));
  EXPECT_FALSE(IsArcTermsOfServiceOobeNegotiationNeeded());

  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kArcBackupRestoreEnabled, std::make_unique<base::Value>(false));
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kArcLocationServiceEnabled, std::make_unique<base::Value>(true));
  EXPECT_FALSE(IsArcTermsOfServiceNegotiationNeeded(profile()));
  EXPECT_FALSE(IsArcTermsOfServiceOobeNegotiationNeeded());

  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kArcBackupRestoreEnabled, std::make_unique<base::Value>(true));
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kArcLocationServiceEnabled, std::make_unique<base::Value>(true));
  EXPECT_FALSE(IsArcTermsOfServiceNegotiationNeeded(profile()));
  EXPECT_FALSE(IsArcTermsOfServiceOobeNegotiationNeeded());
}

TEST_F(ArcOobeTest, ShouldStartArcSilentlyForManagedProfile) {
  base::CommandLine::ForCurrentProcess()->InitFromArgv(
      {"", "--arc-availability=officially-supported"});
  DisableDBusForProfileManager();
  ScopedLogIn login(GetFakeUserManager(),
                    AccountId::FromUserEmailGaiaId(
                        profile()->GetProfileUserName(), kTestGaiaId));

  GetFakeUserManager()->set_current_user_new(true);
  CreateLoginDisplayHost();
  EXPECT_TRUE(IsArcOobeOptInActive());

  SetProfileIsManagedForTesting(profile());
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kArcEnabled, std::make_unique<base::Value>(true));
  EXPECT_FALSE(ShouldStartArcSilentlyForManagedProfile(profile()));

  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kArcBackupRestoreEnabled, std::make_unique<base::Value>(false));
  EXPECT_FALSE(ShouldStartArcSilentlyForManagedProfile(profile()));

  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kArcLocationServiceEnabled, std::make_unique<base::Value>(false));
  EXPECT_TRUE(ShouldStartArcSilentlyForManagedProfile(profile()));

  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kArcBackupRestoreEnabled, std::make_unique<base::Value>(true));
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kArcLocationServiceEnabled, std::make_unique<base::Value>(false));
  EXPECT_TRUE(ShouldStartArcSilentlyForManagedProfile(profile()));

  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kArcBackupRestoreEnabled, std::make_unique<base::Value>(false));
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kArcLocationServiceEnabled, std::make_unique<base::Value>(true));
  EXPECT_TRUE(ShouldStartArcSilentlyForManagedProfile(profile()));

  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kArcBackupRestoreEnabled, std::make_unique<base::Value>(true));
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kArcLocationServiceEnabled, std::make_unique<base::Value>(true));
  EXPECT_TRUE(ShouldStartArcSilentlyForManagedProfile(profile()));
}

using ArcOobeOpaOptInActiveInTest = ArcOobeTest;

TEST_F(ArcOobeOpaOptInActiveInTest, OobeOptInActive) {
  // OOBE OptIn is active in case of OOBE controller is alive and the ARC ToS
  // screen is currently showing.
  EXPECT_FALSE(IsArcOobeOptInActive());
  CreateLoginDisplayHost();
  EXPECT_FALSE(IsArcOobeOptInActive());
  GetFakeUserManager()->set_current_user_new(true);
  EXPECT_TRUE(IsArcOobeOptInActive());
  // OOBE OptIn can be started only for new user flow.
  GetFakeUserManager()->set_current_user_new(false);
  EXPECT_FALSE(IsArcOobeOptInActive());
  // ARC ToS wizard but not for new user.
  login_display_host()->StartWizard(
      chromeos::ArcTermsOfServiceScreenView::kScreenId);
  EXPECT_FALSE(IsArcOobeOptInActive());
}

using DemoSetupFlowArcOptInTest = ArcOobeTest;

TEST_F(DemoSetupFlowArcOptInTest, NoTermsOfServiceOobeNegotiationNeeded) {
  base::CommandLine::ForCurrentProcess()->InitFromArgv(
      {"", "--arc-availability=officially-supported"});
  DisableDBusForProfileManager();
  CreateLoginDisplayHost();
  EXPECT_FALSE(IsArcDemoModeSetupFlow());
  EXPECT_FALSE(IsArcTermsOfServiceOobeNegotiationNeeded());
}

TEST_F(DemoSetupFlowArcOptInTest, TermsOfServiceOobeNegotiationNeeded) {
  base::CommandLine::ForCurrentProcess()->InitFromArgv(
      {"", "--arc-availability=officially-supported"});
  DisableDBusForProfileManager();
  CreateLoginDisplayHost();
  login_display_host()->StartWizard(
      chromeos::DemoPreferencesScreenView::kScreenId);
  login_display_host()
      ->GetWizardController()
      ->SimulateDemoModeSetupForTesting();
  EXPECT_TRUE(IsArcDemoModeSetupFlow());
  EXPECT_TRUE(IsArcTermsOfServiceOobeNegotiationNeeded());
}

TEST_F(DemoSetupFlowArcOptInTest,
       NoPlayStoreNoTermsOfServiceOobeNegotiationNeeded) {
  base::CommandLine::ForCurrentProcess()->InitFromArgv(
      {"", "--arc-availability=officially-supported",
       "--arc-start-mode=always-start-with-no-play-store"});
  DisableDBusForProfileManager();
  CreateLoginDisplayHost();
  login_display_host()->StartWizard(
      chromeos::DemoPreferencesScreenView::kScreenId);
  login_display_host()
      ->GetWizardController()
      ->SimulateDemoModeSetupForTesting();
  EXPECT_TRUE(IsArcDemoModeSetupFlow());
  EXPECT_FALSE(IsArcTermsOfServiceOobeNegotiationNeeded());
}

}  // namespace util
}  // namespace arc
