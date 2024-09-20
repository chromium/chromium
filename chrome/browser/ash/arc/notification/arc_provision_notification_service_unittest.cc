// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/notification/arc_provision_notification_service.h"

#include <memory>
#include <utility>

#include "ash/components/arc/arc_prefs.h"
#include "ash/components/arc/metrics/arc_metrics_service.h"
#include "ash/components/arc/metrics/stability_metrics_manager.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/components/arc/test/arc_util_test_support.h"
#include "ash/components/arc/test/fake_arc_session.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "chrome/browser/ash/arc/arc_optin_uma.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/session/arc_provisioning_result.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/ash/arc/test/test_arc_session_manager.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/ui/ash/login/fake_login_display_host.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "ui/message_center/public/cpp/notification.h"

namespace arc {

namespace {

const char kArcManagedProvisionNotificationId[] = "arc_managed_provision";

class ArcProvisionNotificationServiceTest : public BrowserWithTestWindowTest {
 protected:
  ArcProvisionNotificationServiceTest() = default;
  ArcProvisionNotificationServiceTest(
      const ArcProvisionNotificationServiceTest&) = delete;
  ArcProvisionNotificationServiceTest& operator=(
      const ArcProvisionNotificationServiceTest&) = delete;

  void SetUp() override {
    SetUpInternal(/*should_create_session_manager=*/true);
  }

  void SetUpInternal(bool should_create_session_manager) {
    ash::ConciergeClient::InitializeFake(/*fake_cicerone_client=*/nullptr);

    SetArcAvailableCommandLineForTesting(
        base::CommandLine::ForCurrentProcess());
    ArcSessionManager::SetUiEnabledForTesting(false);

    arc_service_manager_ = std::make_unique<ArcServiceManager>();
    arc_session_manager_ =
        CreateTestArcSessionManager(std::make_unique<ArcSessionRunner>(
            base::BindRepeating(FakeArcSession::Create)));

    if (should_create_session_manager) {
      // SessionManager is created by
      // |AshTestHelper::bluetooth_config_test_helper()|.
      session_manager_ = session_manager::SessionManager::Get();
    }

    // This creates |profile()|, so it has to come after the arc managers.
    BrowserWithTestWindowTest::SetUp();

    arc_service_manager_->set_browser_context(profile());
    display_service_ =
        std::make_unique<NotificationDisplayServiceTester>(profile());
    // Create the service (normally handled by ArcServiceLauncher).
    ArcProvisionNotificationService::GetForBrowserContext(profile());

    arc::prefs::RegisterLocalStatePrefs(local_state_.registry());
    arc::StabilityMetricsManager::Initialize(&local_state_);
  }

  void TearDown() override {
    arc::StabilityMetricsManager::Shutdown();
    // The session manager has to be shutdown before the profile is destroyed so
    // it stops observing prefs, but can't be reset completely because some
    // profile keyed services call into it.
    arc_session_manager_->Shutdown();
    display_service_.reset();
    arc_service_manager_->set_browser_context(nullptr);
    BrowserWithTestWindowTest::TearDown();
    arc_session_manager_.reset();
    arc_service_manager_.reset();

    ash::ConciergeClient::Shutdown();
  }

  std::unique_ptr<ArcServiceManager> arc_service_manager_;
  std::unique_ptr<ArcSessionManager> arc_session_manager_;
  std::unique_ptr<NotificationDisplayServiceTester> display_service_;
  raw_ptr<session_manager::SessionManager> session_manager_;

 private:
  TestingPrefServiceSimple local_state_;
};

}  // namespace

// The managed provision notification is displayed from the beginning of the
// silent opt-in till its successful finish.
TEST_F(ArcProvisionNotificationServiceTest,
       ManagedProvisionNotification_Basic) {
  // Set up managed ARC and assign managed values to all opt-in prefs.
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kArcEnabled, std::make_unique<base::Value>(true));
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kArcBackupRestoreEnabled, std::make_unique<base::Value>(false));
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kArcLocationServiceEnabled, std::make_unique<base::Value>(false));

  arc_session_manager_->SetProfile(profile());
  arc_session_manager_->Initialize();

  // Trigger opt-in flow. The notification gets shown when session starts.
  session_manager_->SetSessionState(
      session_manager::SessionState::LOGIN_PRIMARY);
  arc_session_manager_->RequestEnable();
  EXPECT_FALSE(
      display_service_->GetNotification(kArcManagedProvisionNotificationId));

  session_manager_->SetSessionState(session_manager::SessionState::ACTIVE);
  EXPECT_TRUE(
      display_service_->GetNotification(kArcManagedProvisionNotificationId));
  EXPECT_EQ(ArcSessionManager::State::CHECKING_REQUIREMENTS,
            arc_session_manager_->state());
  arc_session_manager_->StartArcForTesting();
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager_->state());

  // Emulate successful provisioning. The notification gets removed.
  arc::mojom::ArcSignInResultPtr result =
      arc::mojom::ArcSignInResult::NewSuccess(
          arc::mojom::ArcSignInSuccess::SUCCESS);
  arc_session_manager_->OnProvisioningFinished(
      ArcProvisioningResult(std::move(result)));
  EXPECT_FALSE(
      display_service_->GetNotification(kArcManagedProvisionNotificationId));
}

// The managed provision notification is not displayed after the restart if the
// provisioning was successful.
TEST_F(ArcProvisionNotificationServiceTest,
       ManagedProvisionNotification_Restart) {
  // No notifications are expected to be shown in this test.

  // Set up managed ARC and assign managed values to all opt-in prefs.
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kArcEnabled, std::make_unique<base::Value>(true));
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kArcBackupRestoreEnabled, std::make_unique<base::Value>(false));
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kArcLocationServiceEnabled, std::make_unique<base::Value>(false));
  // Set the pref that indicates that signing into ARC has already been
  // performed.
  profile()->GetPrefs()->SetBoolean(prefs::kArcSignedIn, true);

  arc_session_manager_->SetProfile(profile());
  arc_session_manager_->Initialize();

  // Enable ARC. The opt-in flow doesn't take place, and no notification is
  // shown when session starts.
  session_manager_->SetSessionState(
      session_manager::SessionState::LOGIN_PRIMARY);
  arc_session_manager_->AllowActivation(
      ArcSessionManager::AllowActivationReason::kImmediateActivation);
  arc_session_manager_->RequestEnable();
  EXPECT_FALSE(
      display_service_->GetNotification(kArcManagedProvisionNotificationId));

  session_manager_->SetSessionState(session_manager::SessionState::ACTIVE);
  EXPECT_FALSE(
      display_service_->GetNotification(kArcManagedProvisionNotificationId));
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager_->state());
  arc::mojom::ArcSignInResultPtr result =
      arc::mojom::ArcSignInResult::NewSuccess(
          arc::mojom::ArcSignInSuccess::SUCCESS);
  arc_session_manager_->OnProvisioningFinished(
      ArcProvisioningResult(std::move(result)));
  EXPECT_FALSE(
      display_service_->GetNotification(kArcManagedProvisionNotificationId));
}

// The managed provision notification is displayed from the beginning of the
// silent opt-in till the failure of the provision.
TEST_F(ArcProvisionNotificationServiceTest,
       ManagedProvisionNotification_Failure) {
  // Set up managed ARC and assign managed values to all opt-in prefs.
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kArcEnabled, std::make_unique<base::Value>(true));
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kArcBackupRestoreEnabled, std::make_unique<base::Value>(false));
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kArcLocationServiceEnabled, std::make_unique<base::Value>(false));

  arc_session_manager_->SetProfile(profile());
  arc_session_manager_->Initialize();

  // Trigger opt-in flow. The notification gets shown when session starts.
  session_manager_->SetSessionState(
      session_manager::SessionState::LOGIN_PRIMARY);
  arc_session_manager_->RequestEnable();
  EXPECT_FALSE(
      display_service_->GetNotification(kArcManagedProvisionNotificationId));

  session_manager_->SetSessionState(session_manager::SessionState::ACTIVE);
  EXPECT_TRUE(
      display_service_->GetNotification(kArcManagedProvisionNotificationId));
  EXPECT_EQ(ArcSessionManager::State::CHECKING_REQUIREMENTS,
            arc_session_manager_->state());
  arc_session_manager_->StartArcForTesting();
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager_->state());

  // Emulate provisioning failure that leads to stopping ARC. The notification
  // gets removed.
  arc::mojom::ArcSignInResultPtr result = arc::mojom::ArcSignInResult::NewError(
      arc::mojom::ArcSignInError::NewGeneralError(
          arc::mojom::GeneralSignInError::CHROME_SERVER_COMMUNICATION_ERROR));
  arc_session_manager_->OnProvisioningFinished(
      ArcProvisioningResult(std::move(result)));
  EXPECT_FALSE(
      display_service_->GetNotification(kArcManagedProvisionNotificationId));
}

// The managed provision notification is displayed from the beginning of the
// silent opt-in till the failure of the provision.
TEST_F(ArcProvisionNotificationServiceTest,
       ManagedProvisionNotification_FailureNotStopping) {
  // Set up managed ARC and assign managed values to all opt-in prefs.
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kArcEnabled, std::make_unique<base::Value>(true));
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kArcBackupRestoreEnabled, std::make_unique<base::Value>(false));
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kArcLocationServiceEnabled, std::make_unique<base::Value>(false));

  arc_session_manager_->SetProfile(profile());
  arc_session_manager_->Initialize();

  // Trigger opt-in flow. The notification gets shown when session starts.
  session_manager_->SetSessionState(
      session_manager::SessionState::LOGIN_PRIMARY);
  arc_session_manager_->RequestEnable();
  EXPECT_FALSE(
      display_service_->GetNotification(kArcManagedProvisionNotificationId));

  session_manager_->SetSessionState(session_manager::SessionState::ACTIVE);
  EXPECT_TRUE(
      display_service_->GetNotification(kArcManagedProvisionNotificationId));
  EXPECT_EQ(ArcSessionManager::State::CHECKING_REQUIREMENTS,
            arc_session_manager_->state());
  arc_session_manager_->StartArcForTesting();
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager_->state());

  // Emulate provisioning failure that leads to showing an error screen without
  // shutting ARC down. The notification gets removed.
  arc::mojom::ArcSignInResultPtr result = arc::mojom::ArcSignInResult::NewError(
      arc::mojom::ArcSignInError::NewGeneralError(
          arc::mojom::GeneralSignInError::NO_NETWORK_CONNECTION));
  arc_session_manager_->OnProvisioningFinished(
      ArcProvisioningResult(std::move(result)));
  EXPECT_FALSE(
      display_service_->GetNotification(kArcManagedProvisionNotificationId));
}

// The unmanaged provision notification is not displayed.
TEST_F(ArcProvisionNotificationServiceTest,
       UnmanagedProvisionNotification_NotSilent) {
  // No notifications are expected to be shown in this test.

  // Set ARC to be unmanaged.
  SetArcPlayStoreEnabledForProfile(profile(), true);

  arc_session_manager_->SetProfile(profile());
  arc_session_manager_->Initialize();

  // Trigger opt-in flow. The notification is not shown when session starts.
  session_manager_->SetSessionState(
      session_manager::SessionState::LOGIN_PRIMARY);
  arc_session_manager_->RequestEnable();
  EXPECT_FALSE(
      display_service_->GetNotification(kArcManagedProvisionNotificationId));

  session_manager_->SetSessionState(session_manager::SessionState::ACTIVE);
  EXPECT_FALSE(
      display_service_->GetNotification(kArcManagedProvisionNotificationId));
  EXPECT_EQ(ArcSessionManager::State::CHECKING_REQUIREMENTS,
            arc_session_manager_->state());

  // Emulate accepting the terms of service.
  arc_session_manager_->EmulateRequirementCheckCompletionForTesting();
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager_->state());

  // Emulate successful provisioning.
  EXPECT_FALSE(
      display_service_->GetNotification(kArcManagedProvisionNotificationId));
  arc::mojom::ArcSignInResultPtr result =
      arc::mojom::ArcSignInResult::NewSuccess(
          arc::mojom::ArcSignInSuccess::SUCCESS);
  arc_session_manager_->OnProvisioningFinished(
      ArcProvisioningResult(std::move(result)));
  EXPECT_FALSE(
      display_service_->GetNotification(kArcManagedProvisionNotificationId));
}

class ArcProvisionNotificationServiceOobeTest
    : public ArcProvisionNotificationServiceTest {
 protected:
  ArcProvisionNotificationServiceOobeTest() = default;

  ArcProvisionNotificationServiceOobeTest(
      const ArcProvisionNotificationServiceOobeTest&) = delete;
  ArcProvisionNotificationServiceOobeTest& operator=(
      const ArcProvisionNotificationServiceOobeTest&) = delete;

  void SetUp() override {
    // SessionManager is created in FakeLoginDisplayHost. We should not create
    // another one here.
    ArcProvisionNotificationServiceTest::SetUpInternal(
        /*should_create_session_manager=*/false);

    CreateLoginDisplayHost();
  }

  void TearDown() override {
    fake_login_display_host_.reset();
    ArcProvisionNotificationServiceTest::TearDown();
  }

  void CreateLoginDisplayHost() {
    fake_login_display_host_ = std::make_unique<ash::FakeLoginDisplayHost>();
  }

 private:
  std::unique_ptr<ash::FakeLoginDisplayHost> fake_login_display_host_;
};

// For mananged user whose B&R or GLS is not managed, Arc Tos is shown during
// OOBE and no provision notification is expected.
// For the cases B&R or GLS are both managed, OOBE opt-in and in-session opt-in
// have no difference behavior for Arc provision notification.
TEST_F(ArcProvisionNotificationServiceOobeTest,
       ManagedProvisionNotification_NotSilent) {
  // No notifications are expected to be shown in this test.

  EXPECT_TRUE(IsArcOobeOptInActive());
  // Set ARC to be managed.
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kArcEnabled, std::make_unique<base::Value>(true));

  arc_session_manager_->SetProfile(profile());
  arc_session_manager_->Initialize();

  // Trigger opt-in flow. The notification is not shown.
  arc_session_manager_->RequestEnable();
  EXPECT_FALSE(
      display_service_->GetNotification(kArcManagedProvisionNotificationId));
  EXPECT_EQ(ArcSessionManager::State::CHECKING_REQUIREMENTS,
            arc_session_manager_->state());

  // Emulate accepting the terms of service.
  arc_session_manager_->EmulateRequirementCheckCompletionForTesting();
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager_->state());

  // Emulate successful provisioning.
  EXPECT_FALSE(
      display_service_->GetNotification(kArcManagedProvisionNotificationId));
  arc::mojom::ArcSignInResultPtr result =
      arc::mojom::ArcSignInResult::NewSuccess(
          arc::mojom::ArcSignInSuccess::SUCCESS);
  arc_session_manager_->OnProvisioningFinished(
      ArcProvisioningResult(std::move(result)));
  EXPECT_FALSE(
      display_service_->GetNotification(kArcManagedProvisionNotificationId));
}

}  // namespace arc
