// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/child_accounts/website_approval_notifier.h"

#include <memory>
#include <string>

#include "ash/public/cpp/test/test_new_window_delegate.h"
#include "base/memory/ref_counted.h"
#include "base/strings/strcat.h"
#include "base/test/metrics/user_action_tester.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

class WebsiteApprovalNotifierTest : public testing::Test {
 public:
  WebsiteApprovalNotifierTest() = default;
  WebsiteApprovalNotifierTest(const WebsiteApprovalNotifierTest&) = delete;
  WebsiteApprovalNotifierTest& operator=(const WebsiteApprovalNotifierTest&) =
      delete;

  ~WebsiteApprovalNotifierTest() override = default;

 protected:
  void OnNewWebsiteApproval(const std::string& hostname) {
    notifier_.MaybeShowApprovalNotification(hostname);
  }

  std::string GetNotificationId(const std::string& hostname) const {
    return base::StrCat({"website-approval-", hostname});
  }

  bool HasApprovalNotification(const std::string& hostname) const {
    return notification_tester_.GetNotification(GetNotificationId(hostname))
        .has_value();
  }

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  NotificationDisplayServiceTester notification_tester_{&profile_};
  WebsiteApprovalNotifier notifier_{&profile_};
  ash::TestNewWindowDelegateProvider new_window_provider_{
      std::make_unique<ash::TestNewWindowDelegate>()};
};

TEST_F(WebsiteApprovalNotifierTest, ShowNotificationsForValidHosts) {
  std::string host1 = "www.google.com";
  std::string host2 = "images.google.com";
  OnNewWebsiteApproval(host1);
  OnNewWebsiteApproval(host2);
  // Expect both notifications to be shown (no overriding).
  EXPECT_TRUE(HasApprovalNotification(host1));
  EXPECT_TRUE(HasApprovalNotification(host2));
}

TEST_F(WebsiteApprovalNotifierTest, NoNotificationForDomainPattern) {
  std::string host = "*.google.*";
  OnNewWebsiteApproval(host);
  EXPECT_FALSE(HasApprovalNotification(host));
}

TEST_F(WebsiteApprovalNotifierTest, NoNotificationForInvalidHost) {
  std::string host = "google.com:12three";
  OnNewWebsiteApproval(host);
  EXPECT_FALSE(HasApprovalNotification(host));
}

TEST_F(WebsiteApprovalNotifierTest, MetricRecording) {
  base::UserActionTester user_action_tester;
  std::string host = "www.google.com";
  OnNewWebsiteApproval(host);
  EXPECT_TRUE(HasApprovalNotification(host));
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "SupervisedUsers_RemoteWebApproval_NotificationShown"));
  notification_tester_.SimulateClick(NotificationHandler::Type::TRANSIENT,
                                     GetNotificationId(host),
                                     /*action_index=*/absl::nullopt,
                                     /*reply=*/absl::nullopt);
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "SupervisedUsers_RemoteWebApproval_NotificationClicked"));
}

}  // namespace ash
