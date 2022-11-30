// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/help_app/help_app_discover_tab_notification.h"

#include "base/test/metrics/user_action_tester.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/ash/system_web_apps/types/system_web_app_type.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "chrome/browser/notifications/system_notification_helper.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

class HelpAppDiscoverTabNotificationTest : public BrowserWithTestWindowTest {
 public:
  HelpAppDiscoverTabNotificationTest() = default;
  ~HelpAppDiscoverTabNotificationTest() override = default;

  HelpAppDiscoverTabNotificationTest(
      const HelpAppDiscoverTabNotificationTest&) = delete;
  HelpAppDiscoverTabNotificationTest& operator=(
      const HelpAppDiscoverTabNotificationTest&) = delete;

  TestingProfile* CreateProfile() override {
    return profile_manager()->CreateTestingProfile("user@gmail.com");
  }

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    TestingBrowserProcess::GetGlobal()->SetSystemNotificationHelper(
        std::make_unique<SystemNotificationHelper>());
    discover_tab_notification_ =
        std::make_unique<HelpAppDiscoverTabNotification>(profile());
    notification_tester_ =
        std::make_unique<NotificationDisplayServiceTester>(nullptr);
  }

  void TearDown() override {
    discover_tab_notification_.reset();
    notification_tester_.reset();
    BrowserWithTestWindowTest::TearDown();
  }

 protected:
  bool HasDiscoverTabNotification() {
    return notification_tester_
        ->GetNotification(kShowHelpAppDiscoverTabNotificationId)
        .has_value();
  }

  message_center::Notification GetDiscoverTabNotification() {
    return notification_tester_
        ->GetNotification(kShowHelpAppDiscoverTabNotificationId)
        .value();
  }

  std::unique_ptr<HelpAppDiscoverTabNotification> discover_tab_notification_;
  std::unique_ptr<NotificationDisplayServiceTester> notification_tester_;
};

TEST_F(HelpAppDiscoverTabNotificationTest, ShowsNotificationCorrectly) {
  discover_tab_notification_->Show();

  EXPECT_EQ(true, HasDiscoverTabNotification());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_HELP_APP_DISCOVER_TAB_NOTIFICATION_TITLE),
      GetDiscoverTabNotification().title());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_HELP_APP_DISCOVER_TAB_NOTIFICATION_MESSAGE),
      GetDiscoverTabNotification().message());
}

TEST_F(HelpAppDiscoverTabNotificationTest, LogsMetricWhenNotificationShown) {
  base::UserActionTester user_action_tester;

  EXPECT_EQ(0, user_action_tester.GetActionCount(
                   "Discover.DiscoverTabNotification.Shown"));
  discover_tab_notification_->Show();
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "Discover.DiscoverTabNotification.Shown"));
}

TEST_F(HelpAppDiscoverTabNotificationTest, ClickingNotificationDismissesIt) {
  discover_tab_notification_->Show();

  notification_tester_->SimulateClick(NotificationHandler::Type::TRANSIENT,
                                      kShowHelpAppDiscoverTabNotificationId,
                                      /*action_index=*/0,
                                      /*reply=*/absl::nullopt);

  EXPECT_EQ(false, HasDiscoverTabNotification());
}

TEST_F(HelpAppDiscoverTabNotificationTest,
       ClickingNotificationCallsOnClickCallback) {
  base::MockCallback<base::RepeatingClosure> mock_callback;
  EXPECT_CALL(mock_callback, Run());
  discover_tab_notification_->SetOnClickCallbackForTesting(mock_callback.Get());

  discover_tab_notification_->Show();
  notification_tester_->SimulateClick(NotificationHandler::Type::TRANSIENT,
                                      kShowHelpAppDiscoverTabNotificationId,
                                      /*action_index=*/0,
                                      /*reply=*/absl::nullopt);

  EXPECT_EQ(false, HasDiscoverTabNotification());
}

TEST_F(HelpAppDiscoverTabNotificationTest, LogsMetricWhenNotificationClicked) {
  base::UserActionTester user_action_tester;
  discover_tab_notification_->Show();

  EXPECT_EQ(0, user_action_tester.GetActionCount(
                   "Discover.DiscoverTabNotification.Clicked"));
  notification_tester_->SimulateClick(NotificationHandler::Type::TRANSIENT,
                                      kShowHelpAppDiscoverTabNotificationId,
                                      /*action_index=*/0,
                                      /*reply=*/absl::nullopt);
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "Discover.DiscoverTabNotification.Clicked"));
}

}  // namespace ash
