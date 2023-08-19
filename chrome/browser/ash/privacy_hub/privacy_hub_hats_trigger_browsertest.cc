// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/privacy_hub/privacy_hub_hats_trigger.h"

#include "ash/constants/ash_switches.h"
#include "chrome/browser/ash/hats/hats_config.h"
#include "chrome/browser/ash/hats/hats_notification_controller.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/session_manager_types.h"
#include "content/public/test/browser_test.h"

namespace ash {

class PrivacyHubHatsTriggerTest : public InProcessBrowserTest {
 public:
  PrivacyHubHatsTriggerTest() {
    scoped_feature_list_.InitAndEnableFeature(
        kPrivacyHubPostLaunchSurvey.feature);
  }
  ~PrivacyHubHatsTriggerTest() override = default;

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    display_service_ = std::make_unique<NotificationDisplayServiceTester>(
        browser()->profile());
  }

  void SetUpDefaultCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpDefaultCommandLine(command_line);

    command_line->AppendSwitchASCII(
        ash::switches::kForceHappinessTrackingSystem,
        ::features::kHappinessTrackingPrivacyHubPostLaunch.name);
  }

  bool IsHatsNotificationActive() const {
    return display_service_
        ->GetNotification(HatsNotificationController::kNotificationId)
        .has_value();
  }

  const HatsNotificationController* GetHatsNotificationController() const {
    return privacy_hub_trigger_.GetHatsNotificationControllerForTesting();
  }

  base::OneShotTimer& GetTimer() {
    return privacy_hub_trigger_.GetTimerForTesting();
  }

  void SetNoProfileForTesting() {
    privacy_hub_trigger_.SetNoProfileForTesting(true);
  }

  void ExpectTimerIsRunningThenTrigger() {
    EXPECT_TRUE(GetTimer().IsRunning());
    EXPECT_FALSE(IsHatsNotificationActive());
    EXPECT_FALSE(GetHatsNotificationController());

    GetTimer().FireNow();
  }

  void RunThenTriggerTimer(base::TimeDelta delay) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(
            &PrivacyHubHatsTriggerTest::ExpectTimerIsRunningThenTrigger,
            base::Unretained(this)),
        delay);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  PrivacyHubHatsTrigger privacy_hub_trigger_;

  std::unique_ptr<NotificationDisplayServiceTester> display_service_;
};

IN_PROC_BROWSER_TEST_F(PrivacyHubHatsTriggerTest, ShowSurveySuccess) {
  EXPECT_FALSE(IsHatsNotificationActive());

  base::RunLoop loop;
  display_service_->SetNotificationAddedClosure(loop.QuitClosure());
  privacy_hub_trigger_.ShowSurveyAfterDelayElapsed();

  RunThenTriggerTimer(base::Seconds(2));

  loop.Run();

  EXPECT_TRUE(GetHatsNotificationController());
  EXPECT_TRUE(IsHatsNotificationActive());
}

IN_PROC_BROWSER_TEST_F(PrivacyHubHatsTriggerTest, ShowSurveyOnlyOnce) {
  EXPECT_FALSE(IsHatsNotificationActive());

  // Show survey once
  base::RunLoop loop;
  display_service_->SetNotificationAddedClosure(loop.QuitClosure());
  privacy_hub_trigger_.ShowSurveyAfterDelayElapsed();

  RunThenTriggerTimer(base::Seconds(2));

  loop.Run();

  const HatsNotificationController* hats_notification_controller =
      GetHatsNotificationController();
  EXPECT_NE(hats_notification_controller, nullptr);
  EXPECT_TRUE(IsHatsNotificationActive());

  // Trigger survey again but the controller shouldn't be a new instance.
  privacy_hub_trigger_.ShowSurveyAfterDelayElapsed();

  EXPECT_EQ(hats_notification_controller, GetHatsNotificationController());
}

IN_PROC_BROWSER_TEST_F(PrivacyHubHatsTriggerTest, NoActiveProfile) {
  SetNoProfileForTesting();
  EXPECT_FALSE(IsHatsNotificationActive());

  privacy_hub_trigger_.ShowSurveyAfterDelayElapsed();

  EXPECT_FALSE(GetHatsNotificationController());
}

IN_PROC_BROWSER_TEST_F(PrivacyHubHatsTriggerTest, NoSurveyIfSessionNotActive) {
  session_manager::SessionManager::Get()->SetSessionState(
      session_manager::SessionState::LOCKED);
  EXPECT_FALSE(IsHatsNotificationActive());

  privacy_hub_trigger_.ShowSurveyAfterDelayElapsed();

  EXPECT_FALSE(GetHatsNotificationController());
}

}  // namespace ash
