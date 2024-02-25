// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "chrome/browser/ash/bluetooth/hats_bluetooth_revamp_trigger_impl.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/hats_bluetooth_revamp_trigger.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/ash/hats/hats_config.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "chromeos/ash/components/network/network_state.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/session_manager_types.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/browser_test.h"

namespace ash {

namespace {
constexpr char kNotificationId[] = "hats_notification";
const base::TimeDelta kExpectedTimeDelay = base::Minutes(5);
}  // namespace

class HatsBluetoothRevampTriggerTest : public InProcessBrowserTest {
 public:
  HatsBluetoothRevampTriggerTest() {
    scoped_feature_list_.InitAndEnableFeature(
        kHatsBluetoothRevampSurvey.feature);
  }
  ~HatsBluetoothRevampTriggerTest() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    display_service_ = std::make_unique<NotificationDisplayServiceTester>(
        browser()->profile());

    session_manager()->SetSessionState(session_manager::SessionState::ACTIVE);
    bluetooth_revamp_trigger_ = static_cast<HatsBluetoothRevampTriggerImpl*>(
        ash::HatsBluetoothRevampTrigger::Get());
  }

  // InProcessBrowserTest:
  void SetUpDefaultCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpDefaultCommandLine(command_line);

    command_line->AppendSwitchASCII(
        ash::switches::kForceHappinessTrackingSystem,
        ::features::kHappinessTrackingSystemBluetoothRevamp.name);
  }

  void TryToShowSurvey() { bluetooth_revamp_trigger_->TryToShowSurvey(); }

  void SetProfileForTesting() {
    bluetooth_revamp_trigger_->set_profile_for_testing(browser()->profile());
  }

  void SetNullProfileForTesting() {
    bluetooth_revamp_trigger_->set_profile_for_testing(nullptr);
  }

  void WaitForHatsNotification() {
    base::RunLoop loop;
    display_service()->SetNotificationAddedClosure(loop.QuitClosure());
    EXPECT_TRUE(timer()->IsRunning());
    EXPECT_EQ(timer()->GetCurrentDelay(), kExpectedTimeDelay);
    timer()->FireNow();
    loop.Run();
  }

  NotificationDisplayServiceTester* display_service() const {
    return display_service_.get();
  }

  base::OneShotTimer* timer() {
    return bluetooth_revamp_trigger_->timer_for_testing();
  }

  session_manager::SessionManager* session_manager() const {
    return session_manager::SessionManager::Get();
  }

 private:
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
  std::unique_ptr<NotificationDisplayServiceTester> display_service_;
  raw_ptr<HatsBluetoothRevampTriggerImpl, DanglingUntriaged>
      bluetooth_revamp_trigger_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(HatsBluetoothRevampTriggerTest, ShouldShowSurveyTrue) {
  EXPECT_FALSE(display_service()->GetNotification(kNotificationId));
  TryToShowSurvey();
  WaitForHatsNotification();

  // Ensure notification was launched to confirm initialization.
  EXPECT_TRUE(display_service()->GetNotification(kNotificationId));

  // Simulate dismissing notification by the user to clean up the
  // notification.
  display_service()->RemoveNotification(NotificationHandler::Type::TRANSIENT,
                                        kNotificationId, /*by_user=*/true);
}

IN_PROC_BROWSER_TEST_F(HatsBluetoothRevampTriggerTest,
                       ShowSurveyNotCalledIfSessionLocked) {
  session_manager()->SetSessionState(session_manager::SessionState::LOCKED);
  EXPECT_FALSE(display_service()->GetNotification(kNotificationId));

  TryToShowSurvey();

  EXPECT_FALSE(timer()->IsRunning());
  EXPECT_NE(timer()->GetCurrentDelay(), kExpectedTimeDelay);
  EXPECT_FALSE(display_service()->GetNotification(kNotificationId));
}

IN_PROC_BROWSER_TEST_F(HatsBluetoothRevampTriggerTest,
                       ShowSurveyNotCalledIfPrefIsFalse) {
  browser()->profile()->GetPrefs()->SetBoolean(
      ash::prefs::kUserPairedWithFastPair, true);
  EXPECT_FALSE(display_service()->GetNotification(kNotificationId));

  TryToShowSurvey();

  EXPECT_FALSE(timer()->IsRunning());
  EXPECT_NE(timer()->GetCurrentDelay(), kExpectedTimeDelay);
  EXPECT_FALSE(display_service()->GetNotification(kNotificationId));
}

IN_PROC_BROWSER_TEST_F(HatsBluetoothRevampTriggerTest,
                       ShowSurveyNotCalledIfTimerRunning) {
  TryToShowSurvey();

  base::RunLoop loop;
  display_service()->SetNotificationAddedClosure(loop.QuitClosure());

  EXPECT_FALSE(display_service()->GetNotification(kNotificationId));

  EXPECT_TRUE(timer()->IsRunning());
  EXPECT_EQ(timer()->GetCurrentDelay(), kExpectedTimeDelay);

  TryToShowSurvey();
  EXPECT_TRUE(timer()->IsRunning());
  EXPECT_EQ(timer()->GetCurrentDelay(), kExpectedTimeDelay);
  EXPECT_FALSE(display_service()->GetNotification(kNotificationId));

  timer()->FireNow();
  loop.Run();
  EXPECT_TRUE(display_service()->GetNotification(kNotificationId));

  EXPECT_FALSE(timer()->IsRunning());

  // Simulate dismissing notification by the user to clean up the
  // notification.
  display_service()->RemoveNotification(NotificationHandler::Type::TRANSIENT,
                                        kNotificationId, /*by_user=*/true);
}

IN_PROC_BROWSER_TEST_F(HatsBluetoothRevampTriggerTest,
                       ShowSurveyNotCalledWithNoActiveProfile) {
  EXPECT_FALSE(display_service()->GetNotification(kNotificationId));

  SetNullProfileForTesting();
  TryToShowSurvey();
  EXPECT_FALSE(timer()->IsRunning());
  EXPECT_NE(timer()->GetCurrentDelay(), kExpectedTimeDelay);
  EXPECT_FALSE(display_service()->GetNotification(kNotificationId));

  SetProfileForTesting();
  TryToShowSurvey();
  WaitForHatsNotification();

  // Ensure notification was launched to confirm initialization.
  EXPECT_TRUE(display_service()->GetNotification(kNotificationId));

  // Simulate dismissing notification by the user to clean up the
  // notification.
  display_service()->RemoveNotification(NotificationHandler::Type::TRANSIENT,
                                        kNotificationId, /*by_user=*/true);
}

}  // namespace ash
