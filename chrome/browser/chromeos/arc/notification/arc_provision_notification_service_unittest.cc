// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/notification/arc_provision_notification_service.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/values.h"
#include "chrome/browser/chromeos/arc/arc_optin_uma.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/chromeos/arc/session/arc_session_manager.h"
#include "chrome/browser/chromeos/login/ui/fake_login_display_host.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile.h"
#include "components/arc/arc_prefs.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/arc_util.h"
#include "components/arc/test/fake_arc_session.h"
#include "components/prefs/pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_manager.h"
#include "ui/message_center/public/cpp/notification.h"

namespace arc {

namespace {

const char kArcManagedProvisionNotificationId[] = "arc_managed_provision";

class ArcProvisionNotificationServiceTest : public BrowserWithTestWindowTest {
 protected:
  ArcProvisionNotificationServiceTest()
      : user_manager_enabler_(
            std::make_unique<chromeos::FakeChromeUserManager>()) {}

  void SetUp() override {
    SetArcAvailableCommandLineForTesting(
        base::CommandLine::ForCurrentProcess());
    ArcSessionManager::SetUiEnabledForTesting(false);

    arc_service_manager_ = std::make_unique<ArcServiceManager>();
    arc_session_manager_ =
        std::make_unique<ArcSessionManager>(std::make_unique<ArcSessionRunner>(
            base::BindRepeating(FakeArcSession::Create)));

    // This creates |profile()|, so it has to come after the arc managers.
    BrowserWithTestWindowTest::SetUp();

    arc_service_manager_->set_browser_context(profile());
    display_service_ =
        std::make_unique<NotificationDisplayServiceTester>(profile());
    // Create the service (normally handled by ArcServiceLauncher).
    ArcProvisionNotificationService::GetForBrowserContext(profile());

    const AccountId account_id(AccountId::FromUserEmailGaiaId(
        profile()->GetProfileUserName(), "1234567890"));
    GetFakeUserManager()->AddUser(account_id);
    GetFakeUserManager()->LoginUser(account_id);
  }

  void TearDown() override {
    // The session manager has to be shutdown before the profile is destroyed so
    // it stops observing prefs, but can't be reset completely because some
    // profile keyed services call into it.
    arc_session_manager_->Shutdown();
    display_service_.reset();
    arc_service_manager_->set_browser_context(nullptr);
    BrowserWithTestWindowTest::TearDown();
    arc_session_manager_.reset();
    arc_service_manager_.reset();
  }

  chromeos::FakeChromeUserManager* GetFakeUserManager() {
    return static_cast<chromeos::FakeChromeUserManager*>(
        user_manager::UserManager::Get());
  }

  std::unique_ptr<ArcServiceManager> arc_service_manager_;
  std::unique_ptr<ArcSessionManager> arc_session_manager_;
  std::unique_ptr<NotificationDisplayServiceTester> display_service_;

 private:
  user_manager::ScopedUserManager user_manager_enabler_;

  DISALLOW_COPY_AND_ASSIGN(ArcProvisionNotificationServiceTest);
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

  // Trigger opt-in flow. The notification gets shown.
  arc_session_manager_->RequestEnable();
  EXPECT_TRUE(
      display_service_->GetNotification(kArcManagedProvisionNotificationId));
  EXPECT_EQ(ArcSessionManager::State::CHECKING_ANDROID_MANAGEMENT,
            arc_session_manager_->state());
  arc_session_manager_->StartArcForTesting();
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager_->state());

  // Emulate successful provisioning. The notification gets removed.
  arc_session_manager_->OnProvisioningFinished(ProvisioningResult::SUCCESS);
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
  // shown.
  arc_session_manager_->RequestEnable();
  EXPECT_FALSE(
      display_service_->GetNotification(kArcManagedProvisionNotificationId));
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager_->state());
  arc_session_manager_->OnProvisioningFinished(ProvisioningResult::SUCCESS);
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

  // Trigger opt-in flow. The notification gets shown.
  arc_session_manager_->RequestEnable();
  EXPECT_TRUE(
      display_service_->GetNotification(kArcManagedProvisionNotificationId));
  EXPECT_EQ(ArcSessionManager::State::CHECKING_ANDROID_MANAGEMENT,
            arc_session_manager_->state());
  arc_session_manager_->StartArcForTesting();
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager_->state());

  // Emulate provisioning failure that leads to stopping ARC. The notification
  // gets removed.
  arc_session_manager_->OnProvisioningFinished(
      ProvisioningResult::CHROME_SERVER_COMMUNICATION_ERROR);
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

  // Trigger opt-in flow. The notification gets shown.
  arc_session_manager_->RequestEnable();
  EXPECT_TRUE(
      display_service_->GetNotification(kArcManagedProvisionNotificationId));
  EXPECT_EQ(ArcSessionManager::State::CHECKING_ANDROID_MANAGEMENT,
            arc_session_manager_->state());
  arc_session_manager_->StartArcForTesting();
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager_->state());

  // Emulate provisioning failure that leads to showing an error screen without
  // shutting ARC down. The notification gets removed.
  arc_session_manager_->OnProvisioningFinished(
      ProvisioningResult::NO_NETWORK_CONNECTION);
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

  // Trigger opt-in flow. The notification is not shown.
  arc_session_manager_->RequestEnable();
  EXPECT_FALSE(
      display_service_->GetNotification(kArcManagedProvisionNotificationId));
  EXPECT_EQ(ArcSessionManager::State::NEGOTIATING_TERMS_OF_SERVICE,
            arc_session_manager_->state());

  // Emulate accepting the terms of service.
  arc_session_manager_->OnTermsOfServiceNegotiatedForTesting(true);
  arc_session_manager_->StartArcForTesting();
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager_->state());

  // Emulate successful provisioning.
  EXPECT_FALSE(
      display_service_->GetNotification(kArcManagedProvisionNotificationId));
  arc_session_manager_->OnProvisioningFinished(ProvisioningResult::SUCCESS);
  EXPECT_FALSE(
      display_service_->GetNotification(kArcManagedProvisionNotificationId));
}

class ArcProvisionNotificationServiceOobeTest
    : public ArcProvisionNotificationServiceTest {
 protected:
  ArcProvisionNotificationServiceOobeTest() = default;
  void SetUp() override {
    ArcProvisionNotificationServiceTest::SetUp();

    GetFakeUserManager()->set_current_user_new(true);
    CreateLoginDisplayHost();
  }

  void TearDown() override {
    fake_login_display_host_.reset();
    ArcProvisionNotificationServiceTest::TearDown();
  }

  void CreateLoginDisplayHost() {
    fake_login_display_host_ =
        std::make_unique<chromeos::FakeLoginDisplayHost>();
  }

 private:
  std::unique_ptr<chromeos::FakeLoginDisplayHost> fake_login_display_host_;

  DISALLOW_COPY_AND_ASSIGN(ArcProvisionNotificationServiceOobeTest);
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
  EXPECT_EQ(ArcSessionManager::State::NEGOTIATING_TERMS_OF_SERVICE,
            arc_session_manager_->state());

  // Emulate accepting the terms of service.
  arc_session_manager_->OnTermsOfServiceNegotiatedForTesting(true);
  arc_session_manager_->StartArcForTesting();
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager_->state());

  // Emulate successful provisioning.
  EXPECT_FALSE(
      display_service_->GetNotification(kArcManagedProvisionNotificationId));
  arc_session_manager_->OnProvisioningFinished(ProvisioningResult::SUCCESS);
  EXPECT_FALSE(
      display_service_->GetNotification(kArcManagedProvisionNotificationId));
}

}  // namespace arc
