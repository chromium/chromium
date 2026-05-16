// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/bluetooth/hats_bluetooth_revamp_trigger_impl.h"

#include <memory>
#include <string>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/hats_bluetooth_revamp_trigger.h"
#include "base/check_deref.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/ash/hats/hats_config.h"
#include "chrome/browser/ash/hats/hats_notification_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "chromeos/ash/components/network/network_state.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/session_manager_types.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/browser_test.h"
#include "ui/message_center/test/message_center_waiter.h"

namespace ash {

namespace {
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

    session_manager()->SetSessionState(session_manager::SessionState::ACTIVE);
    bluetooth_revamp_trigger_ = static_cast<HatsBluetoothRevampTriggerImpl*>(
        ash::HatsBluetoothRevampTrigger::Get());
  }

  // InProcessBrowserTest:
  void SetUpDefaultCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpDefaultCommandLine(command_line);

    command_line->AppendSwitchASCII(
        ash::switches::kForceHappinessTrackingSystem,
        ash::features::kHappinessTrackingSystemBluetoothRevamp.name);
  }

  void TryToShowSurvey() { bluetooth_revamp_trigger_->TryToShowSurvey(); }

  void SetProfileForTesting() {
    bluetooth_revamp_trigger_->set_profile_for_testing(browser()->profile());
  }

  void SetNullProfileForTesting() {
    bluetooth_revamp_trigger_->set_profile_for_testing(nullptr);
  }

  void WaitForHatsNotification(const std::string& notification_id) {
    EXPECT_TRUE(timer()->IsRunning());
    EXPECT_EQ(timer()->GetCurrentDelay(), kExpectedTimeDelay);
    message_center::MessageCenterWaiter waiter(notification_id);
    timer()->FireNow();
    waiter.WaitUntilAdded();
  }
  message_center::MessageCenter* message_center() const {
    return message_center::MessageCenter::Get();
  }

  base::OneShotTimer* timer() {
    return bluetooth_revamp_trigger_->timer_for_testing();
  }

  session_manager::SessionManager* session_manager() const {
    return session_manager::SessionManager::Get();
  }

  std::string GetHatsNotificationId(const user_manager::User& user) const {
    return HatsNotificationController::GetMessageCenterNotificationIdForTesting(
        user);
  }

  const user_manager::User& GetUserForProfile(Profile* profile) const {
    return CHECK_DEREF(
        BrowserContextHelper::Get()->GetUserByBrowserContext(profile));
  }

 private:
  raw_ptr<HatsBluetoothRevampTriggerImpl, DanglingUntriaged>
      bluetooth_revamp_trigger_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(HatsBluetoothRevampTriggerTest, ShouldShowSurveyTrue) {
  const user_manager::User& user = GetUserForProfile(browser()->profile());
  const std::string notification_id = GetHatsNotificationId(user);
  EXPECT_FALSE(message_center()->FindVisibleNotificationById(notification_id));
  TryToShowSurvey();
  WaitForHatsNotification(notification_id);

  // Ensure notification was launched to confirm initialization.
  EXPECT_TRUE(message_center()->FindVisibleNotificationById(notification_id));

  // Simulate dismissing notification by the user to clean up the
  // notification.
  message_center()->RemoveNotification(notification_id, /*by_user=*/true);
}

IN_PROC_BROWSER_TEST_F(HatsBluetoothRevampTriggerTest,
                       ShowSurveyNotCalledIfSessionLocked) {
  const user_manager::User& user = GetUserForProfile(browser()->profile());
  const std::string notification_id = GetHatsNotificationId(user);
  session_manager()->SetSessionState(session_manager::SessionState::LOCKED);
  EXPECT_FALSE(message_center()->FindVisibleNotificationById(notification_id));

  TryToShowSurvey();

  EXPECT_FALSE(timer()->IsRunning());
  EXPECT_NE(timer()->GetCurrentDelay(), kExpectedTimeDelay);
  EXPECT_FALSE(message_center()->FindVisibleNotificationById(notification_id));
}

IN_PROC_BROWSER_TEST_F(HatsBluetoothRevampTriggerTest,
                       ShowSurveyNotCalledIfPrefIsFalse) {
  const user_manager::User& user = GetUserForProfile(browser()->profile());
  const std::string notification_id = GetHatsNotificationId(user);
  browser()->profile()->GetPrefs()->SetBoolean(
      ash::prefs::kUserPairedWithFastPair, true);
  EXPECT_FALSE(message_center()->FindVisibleNotificationById(notification_id));

  TryToShowSurvey();

  EXPECT_FALSE(timer()->IsRunning());
  EXPECT_NE(timer()->GetCurrentDelay(), kExpectedTimeDelay);
  EXPECT_FALSE(message_center()->FindVisibleNotificationById(notification_id));
}

IN_PROC_BROWSER_TEST_F(HatsBluetoothRevampTriggerTest,
                       ShowSurveyNotCalledIfTimerRunning) {
  const user_manager::User& user = GetUserForProfile(browser()->profile());
  const std::string notification_id = GetHatsNotificationId(user);
  TryToShowSurvey();

  EXPECT_FALSE(message_center()->FindVisibleNotificationById(notification_id));

  EXPECT_TRUE(timer()->IsRunning());
  EXPECT_EQ(timer()->GetCurrentDelay(), kExpectedTimeDelay);

  TryToShowSurvey();
  EXPECT_TRUE(timer()->IsRunning());
  EXPECT_EQ(timer()->GetCurrentDelay(), kExpectedTimeDelay);
  EXPECT_FALSE(message_center()->FindVisibleNotificationById(notification_id));

  WaitForHatsNotification(notification_id);
  EXPECT_TRUE(message_center()->FindVisibleNotificationById(notification_id));

  EXPECT_FALSE(timer()->IsRunning());

  // Simulate dismissing notification by the user to clean up the
  // notification.
  message_center()->RemoveNotification(notification_id, /*by_user=*/true);
}

IN_PROC_BROWSER_TEST_F(HatsBluetoothRevampTriggerTest,
                       ShowSurveyNotCalledWithNoActiveProfile) {
  const user_manager::User& user = GetUserForProfile(browser()->profile());
  const std::string notification_id = GetHatsNotificationId(user);
  EXPECT_FALSE(message_center()->FindVisibleNotificationById(notification_id));

  SetNullProfileForTesting();
  TryToShowSurvey();
  EXPECT_FALSE(timer()->IsRunning());
  EXPECT_NE(timer()->GetCurrentDelay(), kExpectedTimeDelay);
  EXPECT_FALSE(message_center()->FindVisibleNotificationById(notification_id));

  SetProfileForTesting();
  TryToShowSurvey();
  WaitForHatsNotification(notification_id);

  // Ensure notification was launched to confirm initialization.
  EXPECT_TRUE(message_center()->FindVisibleNotificationById(notification_id));

  // Simulate dismissing notification by the user to clean up the
  // notification.
  message_center()->RemoveNotification(notification_id, /*by_user=*/true);
}

}  // namespace ash
