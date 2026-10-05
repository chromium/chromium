// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/instance_throttle/arc_provisioning_throttle_observer.h"

#include <memory>

#include "base/command_line.h"
#include "chrome/browser/ash/arc/session/arc_provisioning_result.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/ash/arc/test/test_arc_session_manager.h"
#include "chrome/browser/ash/login/test/chrome_user_session_test_environment_delegate.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/ash/components/dbus/dlcservice/dlcservice_client.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "chromeos/ash/experiences/arc/arc_prefs.h"
#include "chromeos/ash/experiences/arc/dlc_installer/arc_dlc_installer.h"
#include "chromeos/ash/experiences/arc/mojom/auth.mojom.h"
#include "chromeos/ash/experiences/arc/session/arc_service_manager.h"
#include "chromeos/ash/experiences/arc/session/arc_session_runner.h"
#include "chromeos/ash/experiences/arc/test/arc_util_test_support.h"
#include "chromeos/ash/experiences/arc/test/fake_arc_session.h"
#include "components/account_id/account_id_literal.h"
#include "components/prefs/pref_service.h"
#include "components/services/app_service/public/cpp/app_service_registry.h"
#include "components/session_manager/test/user_session_test_environment.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

namespace {

constexpr AccountId::Literal kTestAccountId =
    AccountId::Literal::FromUserEmailGaiaId("test@test",
                                            GaiaId::Literal("0123456789"));

}  // namespace

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
    ash::DlcserviceClient::InitializeFake();

    auto* browser_process = TestingBrowserProcess::GetGlobal();
    user_session_test_environment_ = std::make_unique<
        ash::test::UserSessionTestEnvironment>(
        browser_process->local_state(),
        std::make_unique<ash::test::ChromeUserSessionTestEnvironmentDelegate>(
            browser_process));
    ASSERT_TRUE(user_session_test_environment_->AddRegularUser(kTestAccountId));

    arc_dlc_installer_ = std::make_unique<ArcDlcInstaller>();
    arc_session_manager_ = CreateTestArcSessionManager(
        std::make_unique<ArcSessionRunner>(
            base::BindRepeating(FakeArcSession::Create)),
        arc_dlc_installer_.get());

    user_session_test_environment_->LogIn(kTestAccountId);
    testing_profile_ = static_cast<TestingProfile*>(Profile::FromBrowserContext(
        ash::BrowserContextHelper::Get()->GetBrowserContextByAccountId(
            kTestAccountId)));
    ASSERT_TRUE(testing_profile_);

    arc_session_manager_->SetProfile(profile());
    arc_session_manager_->Initialize(/*consent_auditor=*/nullptr);
  }

  void TearDown() override {
    observer()->StopObserving();
    arc_session_manager_->Shutdown();

    testing_profile_ = nullptr;
    arc_session_manager_.reset();
    arc_dlc_installer_.reset();
    user_session_test_environment_.reset();
    ash::DlcserviceClient::Shutdown();
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

  apps::AppServiceRegistry app_service_registry_;
  ash::ScopedStubInstallAttributes install_attributes_;
  ash::ScopedTestingCrosSettings testing_cros_settings_;
  std::unique_ptr<ash::test::UserSessionTestEnvironment>
      user_session_test_environment_;
  std::unique_ptr<ArcDlcInstaller> arc_dlc_installer_;
  std::unique_ptr<ArcSessionManager> arc_session_manager_;
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
