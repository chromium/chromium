// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/enterprise/reporting/extension_request/extension_request_notification.h"

#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::HasSubstr;

namespace enterprise_reporting {

constexpr char kChromeWebstoreUrl[] =
    "https://chrome.google.com/webstore/detail/";
constexpr char kFakeExtensionId[] = "fake-extension-id";

// The elements order of array below must match the order in enum
// ExtensionRequestNotification::NotifyType.
const char* const kNotificationIds[] = {"extension_approved_notificaiton",
                                        "extension_rejected_notificaiton",
                                        "extension_installed_notificaiton"};
const char* const kNotificationTitleKeywords[] = {"approved", "rejected",
                                                  "installed"};
const char* const kNotificationBodyKeywords[] = {"to install", "to view",
                                                 "to view"};

void OnNotificationClosed(bool expected_by_user, bool by_user) {
  EXPECT_EQ(expected_by_user, by_user);
}

class ExtensionRequestNotificationTest
    : public BrowserWithTestWindowTest,
      public testing::WithParamInterface<
          ExtensionRequestNotification::NotifyType> {
 public:
  ExtensionRequestNotificationTest() = default;
  ~ExtensionRequestNotificationTest() override = default;

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    display_service_tester_ =
        std::make_unique<NotificationDisplayServiceTester>(profile());
  }

  ExtensionRequestNotification::NotifyType GetNotifyType() {
    return GetParam();
  }

  std::optional<message_center::Notification> GetNotification() {
    return display_service_tester_->GetNotification(
        kNotificationIds[GetNotifyType()]);
  }

  std::unique_ptr<NotificationDisplayServiceTester> display_service_tester_;
};

INSTANTIATE_TEST_SUITE_P(
    ExtensionRequestNotificationTest,
    ExtensionRequestNotificationTest,
    ::testing::Values(ExtensionRequestNotification::kApproved,
                      ExtensionRequestNotification::kRejected,
                      ExtensionRequestNotification::kForceInstalled));

TEST_P(ExtensionRequestNotificationTest, HasExtensionAndClickedByUser) {
  ExtensionRequestNotification request_notification(
      profile(), GetNotifyType(),
      ExtensionRequestNotification::ExtensionIds({kFakeExtensionId}));
  base::RunLoop show_run_loop;
  display_service_tester_->SetNotificationAddedClosure(
      show_run_loop.QuitClosure());
  request_notification.Show(base::BindOnce(&OnNotificationClosed, true));
  show_run_loop.Run();

  std::optional<message_center::Notification> notification = GetNotification();
  ASSERT_TRUE(notification.has_value());

  EXPECT_THAT(base::UTF16ToUTF8(notification->title()),
              HasSubstr(kNotificationTitleKeywords[GetNotifyType()]));
  EXPECT_THAT(base::UTF16ToUTF8(notification->message()),
              HasSubstr(kNotificationBodyKeywords[GetNotifyType()]));

  base::RunLoop close_run_loop;
  display_service_tester_->SetNotificationClosedClosure(
      close_run_loop.QuitClosure());
  display_service_tester_->SimulateClick(
      NotificationHandler::Type::TRANSIENT, kNotificationIds[GetNotifyType()],
      std::optional<int>(), std::optional<std::u16string>());
  close_run_loop.Run();

  EXPECT_FALSE(GetNotification().has_value());
  std::string expected_url =
      std::string(kChromeWebstoreUrl) + std::string(kFakeExtensionId);
  EXPECT_EQ(GURL(expected_url),
            browser()->tab_strip_model()->GetWebContentsAt(0)->GetVisibleURL());
}

TEST_P(ExtensionRequestNotificationTest, HasExtensionAndClosedByBrowser) {
  ExtensionRequestNotification request_notification(
      profile(), GetNotifyType(),
      ExtensionRequestNotification::ExtensionIds({kFakeExtensionId}));
  base::RunLoop show_run_loop;
  display_service_tester_->SetNotificationAddedClosure(
      show_run_loop.QuitClosure());
  request_notification.Show(base::BindOnce(&OnNotificationClosed, false));
  show_run_loop.Run();

  std::optional<message_center::Notification> notification = GetNotification();
  ASSERT_TRUE(notification.has_value());

  base::RunLoop close_run_loop;
  display_service_tester_->SetNotificationClosedClosure(
      close_run_loop.QuitClosure());
  request_notification.CloseNotification();
  close_run_loop.Run();

  EXPECT_FALSE(GetNotification().has_value());
}

}  // namespace enterprise_reporting
