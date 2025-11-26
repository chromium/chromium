// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/session/arc_play_store_enabled_preference_handler.h"

#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/scoped_command_line.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/ash/arc/test/arc_data_removed_waiter.h"
#include "chrome/browser/ash/arc/test/test_arc_session_manager.h"
#include "chrome/browser/ash/login/users/scoped_account_id_annotator.h"
#include "chrome/browser/consent_auditor/consent_auditor_factory.h"
#include "chrome/browser/consent_auditor/consent_auditor_test_utils.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/ash/login/fake_login_display_host.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "chromeos/ash/components/dbus/upstart/upstart_client.h"
#include "chromeos/ash/experiences/arc/arc_prefs.h"
#include "chromeos/ash/experiences/arc/session/arc_session_runner.h"
#include "chromeos/ash/experiences/arc/test/arc_util_test_support.h"
#include "chromeos/ash/experiences/arc/test/fake_arc_session.h"
#include "components/account_id/account_id.h"
#include "components/account_id/account_id_literal.h"
#include "components/consent_auditor/fake_consent_auditor.h"
#include "components/session_manager/core/fake_session_manager_delegate.h"
#include "components/session_manager/core/session_manager.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/user_manager/fake_user_manager_delegate.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/test_helper.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_manager_impl.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ArcPlayTermsOfServiceConsent =
    sync_pb::UserConsentTypes::ArcPlayTermsOfServiceConsent;
using sync_pb::UserConsentTypes;
using testing::_;

namespace arc {
namespace {

constexpr auto kTestAccountId =
    AccountId::Literal::FromUserEmailGaiaId("user@gmail.com",
                                            GaiaId::Literal("1234567890"));

class ArcPlayStoreEnabledPreferenceHandlerTest : public testing::Test {
 public:
  ArcPlayStoreEnabledPreferenceHandlerTest() = default;
  ArcPlayStoreEnabledPreferenceHandlerTest(
      const ArcPlayStoreEnabledPreferenceHandlerTest&) = delete;
  ArcPlayStoreEnabledPreferenceHandlerTest& operator=(
      const ArcPlayStoreEnabledPreferenceHandlerTest&) = delete;
  ~ArcPlayStoreEnabledPreferenceHandlerTest() override = default;

  void SetUp() override {
    ash::ConciergeClient::InitializeFake(/*fake_cicerone_client=*/nullptr);
    ash::SessionManagerClient::InitializeFakeInMemory();
    ash::UpstartClient::InitializeFake();

    SetArcAvailableCommandLineForTesting(
        base::CommandLine::ForCurrentProcess());
    ArcSessionManager::SetUiEnabledForTesting(false);

    ASSERT_TRUE(testing_profile_manager_.SetUp());

    ASSERT_TRUE(user_manager::TestHelper(user_manager_.Get())
                    .AddRegularUser(kTestAccountId));
    user_manager_->UserLoggedIn(
        kTestAccountId,
        user_manager::TestHelper::GetFakeUsernameHash(kTestAccountId));

    ash::ScopedAccountIdAnnotator annotator(
        testing_profile_manager_.profile_manager(), kTestAccountId);
    profile_ = testing_profile_manager_.CreateTestingProfile(
        std::string(kTestAccountId.GetUserEmail()),
        IdentityTestEnvironmentProfileAdaptor ::
            GetIdentityTestEnvironmentFactoriesWithAppendedFactories(
                {TestingProfile::TestingFactory(
                    ConsentAuditorFactory::GetInstance(),
                    base::BindRepeating(&BuildFakeConsentAuditor))}));
    identity_test_env_profile_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile_.get());

    user_manager_->OnUserProfileCreated(kTestAccountId, profile_->GetPrefs());

    arc_session_manager_ =
        CreateTestArcSessionManager(std::make_unique<ArcSessionRunner>(
            base::BindRepeating(FakeArcSession::Create)));
    preference_handler_ =
        std::make_unique<ArcPlayStoreEnabledPreferenceHandler>(
            profile_.get(), arc_session_manager_.get());

    identity_test_env_profile_adaptor_->identity_test_env()
        ->MakePrimaryAccountAvailable(
            std::string(kTestAccountId.GetUserEmail()),
            signin::ConsentLevel::kSignin);
  }

  void TearDown() override {
    preference_handler_.reset();
    arc_session_manager_.reset();
    identity_test_env_profile_adaptor_.reset();

    user_manager_->OnUserProfileWillBeDestroyed(kTestAccountId);
    profile_ = nullptr;
    testing_profile_manager_.DeleteAllTestingProfiles();
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

  GaiaId GetGaiaId() const {
    auto* identity_manager =
        identity_test_env_profile_adaptor_->identity_test_env()
            ->identity_manager();
    return identity_manager
        ->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
        .gaia;
  }

 protected:
  void CreateLoginDisplayHost() {
    fake_login_display_host_ = std::make_unique<ash::FakeLoginDisplayHost>();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  user_manager::ScopedUserManager user_manager_{
      std::make_unique<user_manager::UserManagerImpl>(
          std::make_unique<user_manager::FakeUserManagerDelegate>(),
          TestingBrowserProcess::GetGlobal()->GetTestingLocalState())};
  session_manager::SessionManager session_manager_{
      std::make_unique<session_manager::FakeSessionManagerDelegate>()};
  TestingProfileManager testing_profile_manager_{
      TestingBrowserProcess::GetGlobal()};
  raw_ptr<TestingProfile> profile_ = nullptr;

  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_profile_adaptor_;
  std::unique_ptr<ArcSessionManager> arc_session_manager_;
  std::unique_ptr<ash::FakeLoginDisplayHost> fake_login_display_host_;
  std::unique_ptr<ArcPlayStoreEnabledPreferenceHandler> preference_handler_;
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
  play_consent.set_confirmation_grd_id(IDS_SETTINGS_ANDROID_APPS_REMOVE_BUTTON);
  play_consent.add_description_grd_ids(
      IDS_OS_SETTINGS_ANDROID_APPS_DISABLE_DIALOG_MESSAGE);
  play_consent.set_consent_flow(
      UserConsentTypes::ArcPlayTermsOfServiceConsent::SETTING_CHANGE);
  EXPECT_CALL(*auditor, RecordArcPlayConsent(
                            GetGaiaId(),
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
