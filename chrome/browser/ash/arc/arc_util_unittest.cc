// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/arc_util.h"

#include <memory>

#include "ash/components/arc/arc_features.h"
#include "ash/components/arc/arc_prefs.h"
#include "ash/constants/ash_features.h"
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
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
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
#include "components/account_id/account_id.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_names.h"
#include "components/version_info/version_info.h"
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

class ScopedLogIn {
 public:
  ScopedLogIn(
      ash::FakeChromeUserManager* fake_user_manager,
      const AccountId& account_id,
      user_manager::UserType user_type = user_manager::UserType::kRegular)
      : ScopedLogIn(false, fake_user_manager, account_id, user_type) {}
  ScopedLogIn(
      bool isAffiliated,
      ash::FakeChromeUserManager* fake_user_manager,
      const AccountId& account_id,
      user_manager::UserType user_type = user_manager::UserType::kRegular)
      : fake_user_manager_(fake_user_manager), account_id_(account_id) {
    // Prevent access to DBus. This switch is reset in case set from test SetUp
    // due massive usage of InitFromArgv.
    base::CommandLine& command_line = *base::CommandLine::ForCurrentProcess();
    if (!command_line.HasSwitch(switches::kTestType))
      command_line.AppendSwitch(switches::kTestType);

    switch (user_type) {
      case user_manager::UserType::kRegular:
        if (!isAffiliated)
          LogIn();
        else
          LogInWithAffiliatedAccount();
        break;
      case user_manager::UserType::kPublicAccount:
        LogInAsPublicAccount();
        break;
      default:
        NOTREACHED_IN_MIGRATION();
    }
  }

  ScopedLogIn(const ScopedLogIn&) = delete;
  ScopedLogIn& operator=(const ScopedLogIn&) = delete;

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

  void LogInWithAffiliatedAccount() {
    fake_user_manager_->AddUserWithAffiliation(account_id_, true);
    fake_user_manager_->LoginUser(account_id_);
  }

  raw_ptr<ash::FakeChromeUserManager> fake_user_manager_;
  const AccountId account_id_;
};

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

    fake_user_manager_.Reset(std::make_unique<ash::FakeChromeUserManager>());

    ASSERT_TRUE(data_dir_.CreateUniqueTempDir());
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal(), &local_state_);
    ASSERT_TRUE(profile_manager_->SetUp());

    profile_ = profile_manager_->CreateTestingProfile(kTestProfileName);
  }

  void TearDown() override {
    // Avoid retries, let the next test start safely.
    ResetArcAllowedCheckForTesting(profile_);
    profile_manager_->DeleteTestingProfile(kTestProfileName);
    profile_ = nullptr;
    profile_manager_.reset();
    fake_user_manager_.Reset();
    command_line_.reset();
  }

  TestingProfile* profile() { return profile_; }

  ash::FakeChromeUserManager* GetFakeUserManager() const {
    return fake_user_manager_.Get();
  }

  void LogIn() {
    const auto account_id = AccountId::FromUserEmailGaiaId(
        profile()->GetProfileUserName(), kTestGaiaId);
    fake_user_manager_->AddUser(account_id);
    fake_user_manager_->LoginUser(account_id);
  }

 protected:
  ash::ScopedCrosSettingsTestHelper cros_settings_test_helper_;

 private:
  ScopedTestingLocalState local_state_{TestingBrowserProcess::GetGlobal()};
  std::unique_ptr<base::test::ScopedCommandLine> command_line_;
  base::test::ScopedFeatureList feature_list_;
  content::BrowserTaskEnvironment task_environment_;
  base::ScopedTempDir data_dir_;
  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      fake_user_manager_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  // Owned by |profile_manager_|
  raw_ptr<TestingProfile, DanglingUntriaged> profile_ = nullptr;
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
  EXPECT_FALSE(IsArcAllowedForProfileOnFirstCall(
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true)));
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
  EXPECT_FALSE(IsArcAllowedForProfileOnFirstCall(
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true)));
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
                    user_manager::UserType::kPublicAccount);
  EXPECT_TRUE(IsArcAllowedForProfile(profile()));
}

// Guest account is interpreted as EphemeralDataUser.
TEST_F(ChromeArcUtilTest, IsArcAllowedForProfile_GuestAccount) {
  base::CommandLine::ForCurrentProcess()->InitFromArgv(
      {"", "--arc-availability=officially-supported"});
  ScopedLogIn login(GetFakeUserManager(), user_manager::GuestAccountId());
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
                      user_manager::UserType::kRegular);
    EXPECT_TRUE(IsArcBlockedDueToIncompatibleFileSystem(profile()));
  }

  // Never blocked for a public session.
  {
    ScopedLogIn login(GetFakeUserManager(), robot_id,
                      user_manager::UserType::kPublicAccount);
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
      ash::ProfileHelper::Get()->GetUserByProfile(profile());

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
                    user_manager::UserType::kPublicAccount);
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
                    user_manager::UserType::kPublicAccount);
  EXPECT_FALSE(IsPlayStoreAvailable());
}

TEST_F(ChromeArcUtilTest, ArcStartModeDefaultDemoMode) {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  command_line->InitFromArgv({"", "--arc-availability=installed"});
  cros_settings_test_helper_.InstallAttributes()->SetDemoMode();
  ScopedLogIn login(GetFakeUserManager(),
                    AccountId::FromUserEmail("public_user@gmail.com"),
                    user_manager::UserType::kPublicAccount);
  EXPECT_TRUE(IsPlayStoreAvailable());
}

TEST_F(ChromeArcUtilTest, ArcStartModeDefaultDemoModeWithoutPlayStore) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatureState(ash::features::kShowPlayInDemoMode,
                                    false /* disabled */);
  auto* command_line = base::CommandLine::ForCurrentProcess();
  command_line->InitFromArgv({"", "--arc-availability=installed"});
  cros_settings_test_helper_.InstallAttributes()->SetDemoMode();
  ScopedLogIn login(GetFakeUserManager(),
                    AccountId::FromUserEmail("public_user@gmail.com"),
                    user_manager::UserType::kPublicAccount);
  EXPECT_FALSE(IsPlayStoreAvailable());
}

TEST_F(ChromeArcUtilTest, ArcStartModeWithoutPlayStore) {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  command_line->InitFromArgv(
      {"", "--arc-availability=installed",
       "--arc-start-mode=always-start-with-no-play-store"});
  EXPECT_FALSE(IsPlayStoreAvailable());
}

TEST_F(ChromeArcUtilTest, ArcUnmanagedToManagedTransition_FeatureOn) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      arc::kEnableUnmanagedToManagedTransitionFeature);

  profile()->GetPrefs()->SetInteger(
      arc::prefs::kArcManagementTransition,
      static_cast<int>(arc::ArcManagementTransition::UNMANAGED_TO_MANAGED));

  EXPECT_EQ(GetManagementTransition(profile()),
            arc::ArcManagementTransition::UNMANAGED_TO_MANAGED);
}

TEST_F(ChromeArcUtilTest, ArcUnmanagedToManagedTransition_FeatureOff) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      arc::kEnableUnmanagedToManagedTransitionFeature);

  profile()->GetPrefs()->SetInteger(
      arc::prefs::kArcManagementTransition,
      static_cast<int>(arc::ArcManagementTransition::UNMANAGED_TO_MANAGED));

  EXPECT_EQ(GetManagementTransition(profile()),
            arc::ArcManagementTransition::NO_TRANSITION);
}

class ArcOobeTest : public ChromeArcUtilTest {
 public:
  ArcOobeTest() {
    ash::ConciergeClient::InitializeFake(/*fake_cicerone_client=*/nullptr);
    oobe_configuration_ = std::make_unique<ash::OobeConfiguration>();
  }

  ArcOobeTest(const ArcOobeTest&) = delete;
  ArcOobeTest& operator=(const ArcOobeTest&) = delete;

  ~ArcOobeTest() override {
    // Fake display host have to be shut down first, as it may access
    // configuration.
    fake_login_display_host_.reset();
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
};

TEST_F(ArcOobeTest, TermsOfServiceOobeNegotiationNeededForManagedUser) {
  base::CommandLine::ForCurrentProcess()->InitFromArgv(
      {"", "--arc-availability=officially-supported"});
  DisableDBusForProfileManager();
  ScopedLogIn login(GetFakeUserManager(),
                    AccountId::FromUserEmailGaiaId(
                        profile()->GetProfileUserName(), kTestGaiaId));

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

using ArcOobeOptInActiveInTest = ArcOobeTest;

TEST_F(ArcOobeOptInActiveInTest, OobeOptInActive) {
  // OOBE OptIn is active in case of OOBE controller is alive and the
  // Consolidated Consent screen is currently showing.
  LogIn();
  EXPECT_FALSE(IsArcOobeOptInActive());
  CreateLoginDisplayHost();

  const AccountId account_id = AccountId::FromUserEmailGaiaId(
      profile()->GetProfileUserName(), kTestGaiaId);

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
  login_display_host()->StartWizard(ash::DemoPreferencesScreenView::kScreenId);
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
  base::CommandLine::ForCurrentProcess()->InitFromArgv(
      {"", "--arc-availability=officially-supported"});
  ScopedLogIn login(true, GetFakeUserManager(),
                    AccountId::FromUserEmailGaiaId(
                        profile()->GetProfileUserName(), kTestGaiaId));
  SetProfileIsManagedForTesting(profile());
  profile()->GetPrefs()->SetBoolean(prefs::kUnaffiliatedDeviceArcAllowed,
                                    true);

  EXPECT_TRUE(IsArcAllowedForProfileOnFirstCall(profile()));
}

TEST_F(ChromeUnaffiliatedDevicesArcRestrictionTest,
       ArcAllowedForUnAffiliatedUser_WhenPolicyValueTrue) {
  base::CommandLine::ForCurrentProcess()->InitFromArgv(
      {"", "--arc-availability=officially-supported"});
  ScopedLogIn login(false, GetFakeUserManager(),
                    AccountId::FromUserEmailGaiaId(
                        profile()->GetProfileUserName(), kTestGaiaId));
  SetProfileIsManagedForTesting(profile());
  profile()->GetPrefs()->SetBoolean(prefs::kUnaffiliatedDeviceArcAllowed,
                                    true);

  EXPECT_TRUE(IsArcAllowedForProfileOnFirstCall(profile()));
}

TEST_F(ChromeUnaffiliatedDevicesArcRestrictionTest,
       ArcAllowedForNonEnterpriseAccount_WhenPolicyValueTrue) {
  base::CommandLine::ForCurrentProcess()->InitFromArgv(
      {"", "--arc-availability=officially-supported"});
  ScopedLogIn login(false, GetFakeUserManager(),
                    AccountId::FromUserEmailGaiaId(
                        profile()->GetProfileUserName(), kTestGaiaId));
  profile()->GetPrefs()->SetBoolean(prefs::kUnaffiliatedDeviceArcAllowed,
                                    true);

  EXPECT_TRUE(IsArcAllowedForProfileOnFirstCall(profile()));
}

TEST_F(ChromeUnaffiliatedDevicesArcRestrictionTest,
       ArcAllowedForAffiliatedUser_WhenPolicyValueFalse) {
  base::CommandLine::ForCurrentProcess()->InitFromArgv(
      {"", "--arc-availability=officially-supported"});
  ScopedLogIn login(true, GetFakeUserManager(),
                    AccountId::FromUserEmailGaiaId(
                        profile()->GetProfileUserName(), kTestGaiaId));
  SetProfileIsManagedForTesting(profile());
  profile()->GetPrefs()->SetBoolean(prefs::kUnaffiliatedDeviceArcAllowed,
                                    false);

  EXPECT_TRUE(IsArcAllowedForProfileOnFirstCall(profile()));
}

TEST_F(ChromeUnaffiliatedDevicesArcRestrictionTest,
       ArcNotAllowedForUnAffiliatedUser_WhenPolicyValueFalse) {
  base::CommandLine::ForCurrentProcess()->InitFromArgv(
      {"", "--arc-availability=officially-supported"});
  ScopedLogIn login(false, GetFakeUserManager(),
                    AccountId::FromUserEmailGaiaId(
                        profile()->GetProfileUserName(), kTestGaiaId));
  SetProfileIsManagedForTesting(profile());
  profile()->GetPrefs()->SetBoolean(prefs::kUnaffiliatedDeviceArcAllowed,
                                    false);

  EXPECT_FALSE(IsArcAllowedForProfileOnFirstCall(profile()));
}

TEST_F(ChromeUnaffiliatedDevicesArcRestrictionTest,
       ArcAllowedForNonEnterpriseAccount_WhenPolicyValueFalse) {
  base::CommandLine::ForCurrentProcess()->InitFromArgv(
      {"", "--arc-availability=officially-supported"});
  ScopedLogIn login(false, GetFakeUserManager(),
                    AccountId::FromUserEmailGaiaId(
                        profile()->GetProfileUserName(), kTestGaiaId));
  profile()->GetPrefs()->SetBoolean(prefs::kUnaffiliatedDeviceArcAllowed,
                                    false);

  EXPECT_TRUE(IsArcAllowedForProfileOnFirstCall(profile()));
}

TEST_F(ChromeUnaffiliatedDevicesArcRestrictionTest,
       ReportArcAllowedForAffiliatedUser_WhenPolicyValueFalse) {
  base::HistogramTester tester;
  base::CommandLine::ForCurrentProcess()->InitFromArgv(
      {"", "--arc-availability=officially-supported"});
  ScopedLogIn login(true, GetFakeUserManager(),
                    AccountId::FromUserEmailGaiaId(
                        profile()->GetProfileUserName(), kTestGaiaId));
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
  base::HistogramTester tester;
  base::CommandLine::ForCurrentProcess()->InitFromArgv(
      {"", "--arc-availability=officially-supported"});
  ScopedLogIn login(false, GetFakeUserManager(),
                    AccountId::FromUserEmailGaiaId(
                        profile()->GetProfileUserName(), kTestGaiaId));
  SetProfileIsManagedForTesting(profile());
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kArcEnabled, std::make_unique<base::Value>(true));
  profile()->GetPrefs()->SetBoolean(prefs::kUnaffiliatedDeviceArcAllowed, true);
  RecordArcStatusBasedOnDeviceAffiliationUMA(profile());
  tester.ExpectBucketCount("Arc.Provisioning.DeviceAffiliationAction", 1, 1);
}

TEST_F(ChromeUnaffiliatedDevicesArcRestrictionTest,
       ReportArcNotAllowedForUnAffiliatedUser_WhenPolicyValueFalse) {
  base::HistogramTester tester;
  base::CommandLine::ForCurrentProcess()->InitFromArgv(
      {"", "--arc-availability=officially-supported"});
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kArcEnabled, std::make_unique<base::Value>(true));
  ScopedLogIn login(false, GetFakeUserManager(),
                    AccountId::FromUserEmailGaiaId(
                        profile()->GetProfileUserName(), kTestGaiaId));
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
