// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/instance_throttle/arc_provisioning_throttle_observer.h"

#include <memory>

#include "ash/components/arc/arc_prefs.h"
#include "ash/components/arc/mojom/auth.mojom.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/components/arc/session/arc_session_runner.h"
#include "ash/components/arc/test/arc_util_test_support.h"
#include "ash/components/arc/test/fake_arc_session.h"
#include "base/command_line.h"
#include "chrome/browser/ash/arc/session/arc_provisioning_result.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/ash/arc/test/test_arc_session_manager.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

class ArcProvisioningThrottleObserverTest : public testing::Test {
 public:
  ArcProvisioningThrottleObserverTest()
      : fake_user_manager_(std::make_unique<ash::FakeChromeUserManager>()) {
    ash::ConciergeClient::InitializeFake(/*fake_cicerone_client=*/nullptr);
    SetArcAvailableCommandLineForTesting(
        base::CommandLine::ForCurrentProcess());

    ArcSessionManager::SetUiEnabledForTesting(false);
    ArcSessionManager::SetArcTermsOfServiceOobeNegotiatorEnabledForTesting(
        false);
    ArcSessionManager::EnableCheckAndroidManagementForTesting(false);

    arc_session_manager_ =
        CreateTestArcSessionManager(std::make_unique<ArcSessionRunner>(
            base::BindRepeating(FakeArcSession::Create)));
    testing_profile_ = std::make_unique<TestingProfile>();

    const AccountId account_id(AccountId::FromUserEmailGaiaId(
        testing_profile_->GetProfileUserName(), ""));
    auto* user_manager = static_cast<ash::FakeChromeUserManager*>(
        user_manager::UserManager::Get());
    user_manager->AddUser(account_id);
    user_manager->LoginUser(account_id);

    arc_session_manager_->SetProfile(profile());
    arc_session_manager_->Initialize();
  }

  ArcProvisioningThrottleObserverTest(
      const ArcProvisioningThrottleObserverTest&) = delete;
  ArcProvisioningThrottleObserverTest& operator=(
      const ArcProvisioningThrottleObserverTest&) = delete;

  void TearDown() override {
    observer()->StopObserving();
    arc_session_manager_.reset();
    testing_profile_.reset();
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
  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      fake_user_manager_;
  session_manager::SessionManager session_manager_;
  ArcServiceManager service_manager_;
  std::unique_ptr<ArcSessionManager> arc_session_manager_;
  ArcProvisioningThrottleObserver observer_;
  std::unique_ptr<TestingProfile> testing_profile_;
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
