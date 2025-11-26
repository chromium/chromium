// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/instance_throttle/arc_provisioning_throttle_observer.h"

#include <memory>

#include "base/command_line.h"
#include "chrome/browser/ash/arc/session/arc_provisioning_result.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/ash/arc/test/test_arc_session_manager.h"
#include "chrome/browser/ash/login/users/scoped_account_id_annotator.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/browser_context_helper/annotated_account_id.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/experiences/arc/arc_prefs.h"
#include "chromeos/ash/experiences/arc/mojom/auth.mojom.h"
#include "chromeos/ash/experiences/arc/session/arc_service_manager.h"
#include "chromeos/ash/experiences/arc/session/arc_session_runner.h"
#include "chromeos/ash/experiences/arc/test/arc_util_test_support.h"
#include "chromeos/ash/experiences/arc/test/fake_arc_session.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/fake_session_manager_delegate.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/fake_user_manager_delegate.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/test_helper.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_manager_impl.h"
#include "components/user_manager/user_type.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

class ArcProvisioningThrottleObserverTest : public testing::Test {
 public:
  ArcProvisioningThrottleObserverTest() {
    ash::ConciergeClient::InitializeFake(/*fake_cicerone_client=*/nullptr);
    SetArcAvailableCommandLineForTesting(
        base::CommandLine::ForCurrentProcess());

    ArcSessionManager::SetUiEnabledForTesting(false);
    ArcSessionManager::SetArcTermsOfServiceOobeNegotiatorEnabledForTesting(
        false);
    ArcSessionManager::EnableCheckAndroidManagementForTesting(false);
  }

  ArcProvisioningThrottleObserverTest(
      const ArcProvisioningThrottleObserverTest&) = delete;
  ArcProvisioningThrottleObserverTest& operator=(
      const ArcProvisioningThrottleObserverTest&) = delete;

  void SetUp() override {
    ASSERT_TRUE(testing_profile_manager_.SetUp());

    arc_session_manager_ =
        CreateTestArcSessionManager(std::make_unique<ArcSessionRunner>(
            base::BindRepeating(FakeArcSession::Create)));

    user_manager_.Reset(std::make_unique<user_manager::UserManagerImpl>(
        std::make_unique<user_manager::FakeUserManagerDelegate>(),
        TestingBrowserProcess::GetGlobal()->local_state(),
        ash::CrosSettings::Get()));

    constexpr char kTestingProfileName[] = "test@test";
    const AccountId account_id(AccountId::FromUserEmailGaiaId(
        kTestingProfileName, GaiaId("0123456789")));

    user_manager_->EnsureUser(account_id, user_manager::UserType::kRegular,
                              /*is_ephemeral=*/false);
    user_manager_->UserLoggedIn(
        account_id, user_manager::TestHelper::GetFakeUsernameHash(account_id));

    ash::ScopedAccountIdAnnotator annotator(
        testing_profile_manager_.profile_manager(), account_id);
    testing_profile_ = testing_profile_manager_.CreateTestingProfile(
        TestingProfile::kDefaultProfileUserName);

    arc_session_manager_->SetProfile(profile());
    arc_session_manager_->Initialize();
  }

  void TearDown() override {
    observer()->StopObserving();
    arc_session_manager_->Shutdown();

    testing_profile_ = nullptr;
    testing_profile_manager_.DeleteAllTestingProfiles();

    user_manager_.Reset();
    arc_session_manager_.reset();
  }

 protected:
  ArcProvisioningThrottleObserver* observer() { return &observer_; }

  TestingProfile* profile() { return testing_profile_.get(); }

  void StartObserving() {
    observer()->StartObserving(
        testing_profile_.get(),
        ArcProvisioningThrottleObserver::ObserverStateChangedCallback());
  }

  void StartArc(bool accept_tos) {
    arc_session_manager_->AllowActivation(
        ArcSessionManager::AllowActivationReason::kImmediateActivation);
    arc_session_manager_->RequestEnable();
    if (accept_tos) {
      arc_session_manager_->EmulateRequirementCheckCompletionForTesting();
    }
    DCHECK(arc_session_manager_->state() == ArcSessionManager::State::ACTIVE);
  }

  void StopArc() {
    arc_session_manager_->RequestDisable();
    DCHECK(arc_session_manager_->state() == ArcSessionManager::State::STOPPED);
  }

  void FinishProvisioning() {
    mojom::ArcSignInResultPtr result =
        mojom::ArcSignInResult::NewSuccess(mojom::ArcSignInSuccess::SUCCESS);
    arc_session_manager_->OnProvisioningFinished(
        ArcProvisioningResult(std::move(result)));
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager testing_profile_manager_{
      TestingBrowserProcess::GetGlobal()};
  ash::ScopedStubInstallAttributes install_attributes_;
  ash::ScopedTestingCrosSettings testing_cros_settings_;
  session_manager::SessionManager session_manager_{
      std::make_unique<session_manager::FakeSessionManagerDelegate>()};
  std::unique_ptr<ArcSessionManager> arc_session_manager_;
  user_manager::ScopedUserManager user_manager_;
  ArcServiceManager service_manager_;
  ArcProvisioningThrottleObserver observer_;
  raw_ptr<TestingProfile> testing_profile_ = nullptr;
};

TEST_F(ArcProvisioningThrottleObserverTest, DefaultFlow) {
  StartObserving();

  EXPECT_FALSE(observer()->active());

  StartArc(true /* accept_tos */);
  EXPECT_TRUE(observer()->active());
  FinishProvisioning();
  EXPECT_FALSE(observer()->active());
}

TEST_F(ArcProvisioningThrottleObserverTest, AlreadyProvisionedStart) {
  profile()->GetPrefs()->SetBoolean(prefs::kArcSignedIn, true);

  StartObserving();

  EXPECT_FALSE(observer()->active());
  StartArc(false /* accept_tos */);
  EXPECT_FALSE(observer()->active());

  // Handler optout/optin in the same session.
  StopArc();
  EXPECT_FALSE(observer()->active());
  profile()->GetPrefs()->SetBoolean(prefs::kArcSignedIn, false);
  StartArc(true /* accept_tos */);
  EXPECT_TRUE(observer()->active());
}

}  // namespace arc
