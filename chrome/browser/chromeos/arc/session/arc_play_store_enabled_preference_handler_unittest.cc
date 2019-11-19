// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/session/arc_play_store_enabled_preference_handler.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/chromeos/arc/session/arc_session_manager.h"
#include "chrome/browser/chromeos/arc/test/arc_data_removed_waiter.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/consent_auditor/consent_auditor_factory.h"
#include "chrome/browser/consent_auditor/consent_auditor_test_utils.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/session_manager/session_manager_client.h"
#include "chromeos/dbus/upstart/upstart_client.h"
#include "components/arc/arc_prefs.h"
#include "components/arc/arc_util.h"
#include "components/arc/session/arc_session_runner.h"
#include "components/arc/test/fake_arc_session.h"
#include "components/consent_auditor/fake_consent_auditor.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
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
      : user_manager_enabler_(
            std::make_unique<chromeos::FakeChromeUserManager>()) {}

  void SetUp() override {
    chromeos::SessionManagerClient::InitializeFakeInMemory();
    chromeos::UpstartClient::InitializeFake();

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
        std::make_unique<ArcSessionManager>(std::make_unique<ArcSessionRunner>(
            base::BindRepeating(FakeArcSession::Create)));
    preference_handler_ =
        std::make_unique<ArcPlayStoreEnabledPreferenceHandler>(
            profile_.get(), arc_session_manager_.get());
    const AccountId account_id(AccountId::FromUserEmailGaiaId(
        profile()->GetProfileUserName(), kTestGaiaId));
    GetFakeUserManager()->AddUser(account_id);
    GetFakeUserManager()->LoginUser(account_id);

    identity_test_env_profile_adaptor_->identity_test_env()->SetPrimaryAccount(
        kTestEmail);
  }

  void TearDown() override {
    preference_handler_.reset();
    arc_session_manager_.reset();
    identity_test_env_profile_adaptor_.reset();
    profile_.reset();
    chromeos::UpstartClient::Shutdown();
    chromeos::SessionManagerClient::Shutdown();
  }

  TestingProfile* profile() const { return profile_.get(); }
  ArcSessionManager* arc_session_manager() const {
    return arc_session_manager_.get();
  }
  ArcPlayStoreEnabledPreferenceHandler* preference_handler() const {
    return preference_handler_.get();
  }
  chromeos::FakeChromeUserManager* GetFakeUserManager() const {
    return static_cast<chromeos::FakeChromeUserManager*>(
        user_manager::UserManager::Get());
  }

  consent_auditor::FakeConsentAuditor* consent_auditor() const {
    return static_cast<consent_auditor::FakeConsentAuditor*>(
        ConsentAuditorFactory::GetForProfile(profile()));
  }

  CoreAccountId GetAuthenticatedAccountId() const {
    auto* identity_manager =
        identity_test_env_profile_adaptor_->identity_test_env()
            ->identity_manager();
    return identity_manager->GetPrimaryAccountInfo().account_id;
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  user_manager::ScopedUserManager user_manager_enabler_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_profile_adaptor_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<ArcSessionManager> arc_session_manager_;
  std::unique_ptr<ArcPlayStoreEnabledPreferenceHandler> preference_handler_;

  DISALLOW_COPY_AND_ASSIGN(ArcPlayStoreEnabledPreferenceHandlerTest);
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
  ASSERT_EQ(ArcSessionManager::State::NEGOTIATING_TERMS_OF_SERVICE,
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
  ASSERT_EQ(ArcSessionManager::State::NEGOTIATING_TERMS_OF_SERVICE,
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
                            GetAuthenticatedAccountId(),
                            consent_auditor::ArcPlayConsentEq(play_consent)));

  ASSERT_FALSE(IsArcPlayStoreEnabledForProfile(profile()));
  arc_session_manager()->SetProfile(profile());
  arc_session_manager()->Initialize();
  preference_handler()->Start();

  SetArcPlayStoreEnabledForProfile(profile(), true);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(ArcSessionManager::State::NEGOTIATING_TERMS_OF_SERVICE,
            arc_session_manager()->state());

  SetArcPlayStoreEnabledForProfile(profile(), false);
}

}  // namespace
}  // namespace arc
