// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/session/arc_play_store_enabled_preference_handler.h"

#include <memory>
#include <string>

#include "ash/components/arc/arc_prefs.h"
#include "ash/components/arc/session/arc_session_runner.h"
#include "ash/components/arc/test/arc_util_test_support.h"
#include "ash/components/arc/test/fake_arc_session.h"
#include "base/command_line.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/scoped_command_line.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/ash/arc/test/arc_data_removed_waiter.h"
#include "chrome/browser/ash/arc/test/test_arc_session_manager.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/consent_auditor/consent_auditor_factory.h"
#include "chrome/browser/consent_auditor/consent_auditor_test_utils.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/ash/login/fake_login_display_host.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "chromeos/ash/components/dbus/upstart/upstart_client.h"
#include "components/consent_auditor/fake_consent_auditor.h"
#include "components/session_manager/core/session_manager.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ArcPlayTermsOfServiceConsent =
    sync_pb::UserConsentTypes::ArcPlayTermsOfServiceConsent;
using sync_pb::UserConsentTypes;
using testing::_;

namespace arc {
namespace {

constexpr char kTestEmail[] = "user@gmail.com";
constexpr char kTestGaiaId[] = "1234567890";

class ArcPlayStoreEnabledPreferenceHandlerTest : public testing::Test {
 public:
  ArcPlayStoreEnabledPreferenceHandlerTest()
      : fake_user_manager_(std::make_unique<ash::FakeChromeUserManager>()) {}

  ArcPlayStoreEnabledPreferenceHandlerTest(
      const ArcPlayStoreEnabledPreferenceHandlerTest&) = delete;
  ArcPlayStoreEnabledPreferenceHandlerTest& operator=(
      const ArcPlayStoreEnabledPreferenceHandlerTest&) = delete;

  void SetUp() override {
    ash::ConciergeClient::InitializeFake(/*fake_cicerone_client=*/nullptr);
    ash::SessionManagerClient::InitializeFakeInMemory();
    ash::UpstartClient::InitializeFake();

    SetArcAvailableCommandLineForTesting(
        base::CommandLine::ForCurrentProcess());
    ArcSessionManager::SetUiEnabledForTesting(false);

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    TestingProfile::Builder profile_builder;
    profile_builder.SetProfileName(kTestEmail);
    profile_builder.SetPath(temp_dir_.GetPath().AppendASCII("TestArcProfile"));
    profile_builder.AddTestingFactory(
        ConsentAuditorFactory::GetInstance(),
        base::BindRepeating(&BuildFakeConsentAuditor));
    profile_ = IdentityTestEnvironmentProfileAdaptor::
        CreateProfileForIdentityTestEnvironment(profile_builder);
    identity_test_env_profile_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile_.get());

    arc_session_manager_ =
        CreateTestArcSessionManager(std::make_unique<ArcSessionRunner>(
            base::BindRepeating(FakeArcSession::Create)));
    preference_handler_ =
        std::make_unique<ArcPlayStoreEnabledPreferenceHandler>(
            profile_.get(), arc_session_manager_.get());
    const AccountId account_id(AccountId::FromUserEmailGaiaId(
        profile()->GetProfileUserName(), kTestGaiaId));
    fake_user_manager_->AddUser(account_id);
    fake_user_manager_->LoginUser(account_id);

    identity_test_env_profile_adaptor_->identity_test_env()
        ->MakePrimaryAccountAvailable(kTestEmail,
                                      signin::ConsentLevel::kSignin);

    TestingBrowserProcess::GetGlobal()->SetLocalState(&pref_service_);
    user_manager::KnownUser::RegisterPrefs(pref_service_.registry());
  }

  void TearDown() override {
    preference_handler_.reset();
    arc_session_manager_.reset();
    identity_test_env_profile_adaptor_.reset();
    profile_.reset();
    TestingBrowserProcess::GetGlobal()->SetLocalState(nullptr);
    ash::UpstartClient::Shutdown();
    ash::SessionManagerClient::Shutdown();
    ash::ConciergeClient::Shutdown();
  }

  TestingProfile* profile() const { return profile_.get(); }
  ArcSessionManager* arc_session_manager() const {
    return arc_session_manager_.get();
  }
  ArcPlayStoreEnabledPreferenceHandler* preference_handler() const {
    return preference_handler_.get();
  }

  consent_auditor::FakeConsentAuditor* consent_auditor() const {
    return static_cast<consent_auditor::FakeConsentAuditor*>(
        ConsentAuditorFactory::GetForProfile(profile()));
  }

  CoreAccountId GetAccountId() const {
    auto* identity_manager =
        identity_test_env_profile_adaptor_->identity_test_env()
            ->identity_manager();
    return identity_manager
        ->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
        .account_id;
  }

 protected:
  void CreateLoginDisplayHost() {
    fake_login_display_host_ = std::make_unique<ash::FakeLoginDisplayHost>();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      fake_user_manager_;
  session_manager::SessionManager session_manager_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_profile_adaptor_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<ArcSessionManager> arc_session_manager_;
  std::unique_ptr<ash::FakeLoginDisplayHost> fake_login_display_host_;
  std::unique_ptr<ArcPlayStoreEnabledPreferenceHandler> preference_handler_;
  TestingPrefServiceSimple pref_service_;
};

TEST_F(ArcPlayStoreEnabledPreferenceHandlerTest, PrefChangeTriggersService) {
  ASSERT_FALSE(IsArcPlayStoreEnabledForProfile(profile()));
  arc_session_manager()->SetProfile(profile());
  arc_session_manager()->Initialize();
  preference_handler()->Start();

  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(prefs::kArcDataRemoveRequested));
  EXPECT_EQ(ArcSessionManager::State::STOPPED, arc_session_manager()->state());

  SetArcPlayStoreEnabledForProfile(profile(), true);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(ArcSessionManager::State::CHECKING_REQUIREMENTS,
            arc_session_manager()->state());

  SetArcPlayStoreEnabledForProfile(profile(), false);

  ArcDataRemovedWaiter().Wait();
  ASSERT_EQ(ArcSessionManager::State::STOPPED, arc_session_manager()->state());
}

TEST_F(ArcPlayStoreEnabledPreferenceHandlerTest,
       PrefChangeTriggersService_Restart) {
  // Sets the Google Play Store preference at beginning.
  SetArcPlayStoreEnabledForProfile(profile(), true);

  arc_session_manager()->SetProfile(profile());
  arc_session_manager()->Initialize();
  preference_handler()->Start();

  // Setting profile initiates a code fetching process.
  ASSERT_EQ(ArcSessionManager::State::CHECKING_REQUIREMENTS,
            arc_session_manager()->state());
}

TEST_F(ArcPlayStoreEnabledPreferenceHandlerTest, RemoveDataDir_Managed) {
  // Set ARC to be managed and disabled.
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kArcEnabled, std::make_unique<base::Value>(false));

  // Starting session manager with prefs::kArcEnabled off in a managed profile
  // does automatically remove Android's data folder.
  arc_session_manager()->SetProfile(profile());
  arc_session_manager()->Initialize();
  preference_handler()->Start();
  EXPECT_TRUE(
      profile()->GetPrefs()->GetBoolean(prefs::kArcDataRemoveRequested));
}

TEST_F(ArcPlayStoreEnabledPreferenceHandlerTest, PrefChangeRevokesConsent) {
  consent_auditor::FakeConsentAuditor* auditor = consent_auditor();

  ArcPlayTermsOfServiceConsent play_consent;
  play_consent.set_status(UserConsentTypes::NOT_GIVEN);
  play_consent.set_confirmation_grd_id(
      IDS_SETTINGS_ANDROID_APPS_DISABLE_DIALOG_REMOVE);
  play_consent.add_description_grd_ids(
      IDS_SETTINGS_ANDROID_APPS_DISABLE_DIALOG_MESSAGE);
  play_consent.set_consent_flow(
      UserConsentTypes::ArcPlayTermsOfServiceConsent::SETTING_CHANGE);
  EXPECT_CALL(*auditor, RecordArcPlayConsent(
                            GetAccountId(),
                            consent_auditor::ArcPlayConsentEq(play_consent)));

  ASSERT_FALSE(IsArcPlayStoreEnabledForProfile(profile()));
  arc_session_manager()->SetProfile(profile());
  arc_session_manager()->Initialize();
  preference_handler()->Start();

  SetArcPlayStoreEnabledForProfile(profile(), true);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(ArcSessionManager::State::CHECKING_REQUIREMENTS,
            arc_session_manager()->state());

  SetArcPlayStoreEnabledForProfile(profile(), false);
}

// This verifies ARC start logic in case ARC manual start is activated.
// It is expected that setting Play Store enabled preference does not
// automatically start ARC as it is done by default.
TEST_F(ArcPlayStoreEnabledPreferenceHandlerTest, ManualStart) {
  SetArcAvailableCommandLineForTesting(base::CommandLine::ForCurrentProcess());
  base::test::ScopedCommandLine command_line;
  command_line.GetProcessCommandLine()->AppendSwitchASCII("arc-start-mode",
                                                          "manual");

  arc_session_manager()->SetProfile(profile());
  arc_session_manager()->Initialize();
  preference_handler()->Start();

  // ARC is neither enabled by preference nor by manual start.
  EXPECT_EQ(ArcSessionManager::State::STOPPED, arc_session_manager()->state());

  SetArcPlayStoreEnabledForProfile(profile(), true);

  // ARC is enabled by preference but automatic ARC start is blocked by
  // manual mode.
  EXPECT_EQ(ArcSessionManager::State::STOPPED, arc_session_manager()->state());

  arc_session_manager()->RequestEnable();

  // Now ARC started by manual request.
  EXPECT_EQ(ArcSessionManager::State::CHECKING_REQUIREMENTS,
            arc_session_manager()->state());
}

// Similar by |ManualStart| above but verifies that ARC manual start ignores
// Play Store enabled preference.
TEST_F(ArcPlayStoreEnabledPreferenceHandlerTest, ManualStartIgnorePreference) {
  SetArcAvailableCommandLineForTesting(base::CommandLine::ForCurrentProcess());
  base::test::ScopedCommandLine command_line;
  command_line.GetProcessCommandLine()->AppendSwitchASCII("arc-start-mode",
                                                          "manual");

  arc_session_manager()->SetProfile(profile());
  arc_session_manager()->Initialize();
  preference_handler()->Start();

  // ARC is neither enabled by preference nor by manual start.
  EXPECT_EQ(ArcSessionManager::State::STOPPED, arc_session_manager()->state());

  arc_session_manager()->RequestEnable();

  // Now ARC started by manual request even if Play Store enabled preference
  // was not set.
  EXPECT_FALSE(IsArcPlayStoreEnabledForProfile(profile()));
  EXPECT_EQ(ArcSessionManager::State::CHECKING_REQUIREMENTS,
            arc_session_manager()->state());
}

// Verifies that ARC manual start disables ARC automatic start for already
// provisioned state.
TEST_F(ArcPlayStoreEnabledPreferenceHandlerTest,
       ManualStartAlreadyProvisioned) {
  SetArcAvailableCommandLineForTesting(base::CommandLine::ForCurrentProcess());
  base::test::ScopedCommandLine command_line;
  command_line.GetProcessCommandLine()->AppendSwitchASCII("arc-start-mode",
                                                          "manual");

  profile()->GetPrefs()->SetBoolean(prefs::kArcSignedIn, true);

  // Sets the Google Play Store preference at beginning.
  SetArcPlayStoreEnabledForProfile(profile(), true);

  arc_session_manager()->SetProfile(profile());
  arc_session_manager()->Initialize();
  preference_handler()->Start();

  // ARC is enable and already provisoned by manual mode blocks the start.
  ASSERT_EQ(ArcSessionManager::State::STOPPED, arc_session_manager()->state());

  arc_session_manager()->AllowActivation(
      ArcSessionManager::AllowActivationReason::kImmediateActivation);
  arc_session_manager()->RequestEnable();

  // Now ARC started by manual request.
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());
}

TEST_F(ArcPlayStoreEnabledPreferenceHandlerTest, MiniStateUnmanaged) {
  // Ensure the mini-instance starts.
  SetArcAvailableCommandLineForTesting(base::CommandLine::ForCurrentProcess());
  ash::SessionManagerClient::Get()->EmitLoginPromptVisible();
  ASSERT_TRUE(arc_session_manager()
                  ->GetArcSessionRunnerForTesting()
                  ->GetArcSessionForTesting());
  ASSERT_EQ(ArcSessionManager::State::NOT_INITIALIZED,
            arc_session_manager()->state());

  // Take new user through OOBE.
  CreateLoginDisplayHost();
  ASSERT_TRUE(IsArcOobeOptInActive());

  arc_session_manager()->SetProfile(profile());
  arc_session_manager()->Initialize();
  preference_handler()->Start();

  // Ensure that we are still in mini instance.
  EXPECT_TRUE(arc_session_manager()
                  ->GetArcSessionRunnerForTesting()
                  ->GetArcSessionForTesting());
  EXPECT_EQ(ArcSessionManager::State::STOPPED, arc_session_manager()->state());
}

TEST_F(ArcPlayStoreEnabledPreferenceHandlerTest, MiniStateManagedDisabled) {
  // Ensure the mini-instance starts.
  SetArcAvailableCommandLineForTesting(base::CommandLine::ForCurrentProcess());
  ash::SessionManagerClient::Get()->EmitLoginPromptVisible();
  ASSERT_TRUE(arc_session_manager()
                  ->GetArcSessionRunnerForTesting()
                  ->GetArcSessionForTesting());
  ASSERT_EQ(ArcSessionManager::State::NOT_INITIALIZED,
            arc_session_manager()->state());

  // Take new user through OOBE.
  CreateLoginDisplayHost();
  ASSERT_TRUE(IsArcOobeOptInActive());

  // Set ARC to be managed and disabled.
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kArcEnabled, std::make_unique<base::Value>(false));

  arc_session_manager()->SetProfile(profile());
  arc_session_manager()->Initialize();
  preference_handler()->Start();

  // Ensure that we stop the mini instance.
  EXPECT_FALSE(arc_session_manager()
                   ->GetArcSessionRunnerForTesting()
                   ->GetArcSessionForTesting());
}

TEST_F(ArcPlayStoreEnabledPreferenceHandlerTest, MiniStateManagedEnabled) {
  // Ensure the mini-instance starts.
  SetArcAvailableCommandLineForTesting(base::CommandLine::ForCurrentProcess());
  ash::SessionManagerClient::Get()->EmitLoginPromptVisible();
  ASSERT_TRUE(arc_session_manager()
                  ->GetArcSessionRunnerForTesting()
                  ->GetArcSessionForTesting());
  ASSERT_EQ(ArcSessionManager::State::NOT_INITIALIZED,
            arc_session_manager()->state());

  // Take new user through OOBE.
  CreateLoginDisplayHost();
  ASSERT_TRUE(IsArcOobeOptInActive());

  // Set ARC to be managed and enabled.
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kArcEnabled, std::make_unique<base::Value>(true));

  arc_session_manager()->SetProfile(profile());
  arc_session_manager()->Initialize();
  preference_handler()->Start();

  // Ensure do do not stop the mini instance.
  EXPECT_TRUE(arc_session_manager()
                  ->GetArcSessionRunnerForTesting()
                  ->GetArcSessionForTesting());
  EXPECT_EQ(ArcSessionManager::State::CHECKING_REQUIREMENTS,
            arc_session_manager()->state());
}

}  // namespace
}  // namespace arc
