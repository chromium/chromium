// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/reporting/extension_request/extension_request_notification.h"

#include <array>

#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::HasSubstr;

namespace enterprise_reporting {

constexpr char kChromeWebstoreUrl[] =
    "https://chrome.google.com/webstore/detail/";
constexpr char kFakeExtensionId[] = "fake-extension-id";

// The elements order of array below must match the order in enum
// ExtensionRequestNotification::NotifyType.
constexpr auto kNotificationIds = std::to_array<const char*>({
    "extension_approved_notificaiton",
    "extension_rejected_notificaiton",
    "extension_installed_notificaiton",
});
constexpr auto kNotificationTitleKeywords = std::to_array<const char*>({
    "approved",
    "rejected",
    "installed",
});
constexpr auto kNotificationBodyKeywords = std::to_array<const char*>({
    "to install",
    "to view",
    "to view",
});

void OnNotificationClosed(bool expected_by_user, bool by_user) {
  EXPECT_EQ(expected_by_user, by_user);
}

class ExtensionRequestNotificationTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<
          ExtensionRequestNotification::NotifyType> {
 public:
  ExtensionRequestNotificationTest() = default;
  ~ExtensionRequestNotificationTest() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    display_service_tester_ =
        std::make_unique<NotificationDisplayServiceTester>(GetProfile());
  }

  void TearDownOnMainThread() override {
    display_service_tester_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
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

IN_PROC_BROWSER_TEST_P(ExtensionRequestNotificationTest,
                       HasExtensionAndClickedByUser) {
  ExtensionRequestNotification request_notification(
      GetProfile(), GetNotifyType(),
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
  EXPECT_EQ(
      GURL(expected_url),
      browser()->tab_strip_model()->GetActiveWebContents()->GetVisibleURL());
}

IN_PROC_BROWSER_TEST_P(ExtensionRequestNotificationTest,
                       HasExtensionAndClosedByBrowser) {
  ExtensionRequestNotification request_notification(
      GetProfile(), GetNotifyType(),
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
