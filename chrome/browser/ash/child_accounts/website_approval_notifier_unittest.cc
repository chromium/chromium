// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/child_accounts/website_approval_notifier.h"

#include <memory>
#include <string>

#include "ash/public/cpp/notification_utils.h"
#include "ash/public/cpp/test/test_new_window_delegate.h"
#include "base/check_deref.h"
#include "base/memory/ref_counted.h"
#include "base/strings/strcat.h"
#include "base/test/metrics/user_action_tester.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/browser_context_helper/annotated_account_id.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_names.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"

namespace ash {

namespace {
// A mock implementation of |NewWindowDelegate| for use in tests.
class MockNewWindowDelegate : public testing::NiceMock<TestNewWindowDelegate> {
 public:
  // TestNewWindowDelegate:
  MOCK_METHOD(void,
              OpenUrl,
              (const GURL& url, OpenUrlFrom from, Disposition disposition),
              (override));
};
}  // namespace

class WebsiteApprovalNotifierTest : public testing::Test {
 public:
  WebsiteApprovalNotifierTest() = default;
  WebsiteApprovalNotifierTest(const WebsiteApprovalNotifierTest&) = delete;
  WebsiteApprovalNotifierTest& operator=(const WebsiteApprovalNotifierTest&) =
      delete;

  ~WebsiteApprovalNotifierTest() override = default;

  void SetUp() override {
    message_center::MessageCenter::Initialize();
    user_manager::User* user =
        fake_user_manager_->AddUser(user_manager::StubAccountId());
    fake_user_manager_->LoginUser(user->GetAccountId());
    AnnotatedAccountId::Set(&profile_, user->GetAccountId());
    notifier_ = std::make_unique<WebsiteApprovalNotifier>(&profile_);
  }

  void TearDown() override {
    notifier_.reset();
    message_center::MessageCenter::Shutdown();
  }

  MockNewWindowDelegate& new_window_delegate() { return new_window_delegate_; }

 protected:
  void OnNewWebsiteApproval(const std::string& hostname) {
    notifier_->MaybeShowApprovalNotification(hostname);
  }

  std::string GetNotificationId(const std::string& hostname) const {
    return base::StrCat({"website-approval-", hostname});
  }

  const message_center::Notification* GetApprovalNotification(
      const std::string& hostname) {
    const user_manager::User& user = CHECK_DEREF(
        BrowserContextHelper::Get()->GetUserByBrowserContext(&profile_));
    return message_center::MessageCenter::Get()->FindNotificationById(
        CreateUserScopedNotificationId(GetNotificationId(hostname),
                                       user.username_hash()));
  }

  content::BrowserTaskEnvironment task_environment_;
  user_manager::TypedScopedUserManager<FakeChromeUserManager>
      fake_user_manager_{std::make_unique<FakeChromeUserManager>()};
  TestingProfile profile_;
  std::unique_ptr<WebsiteApprovalNotifier> notifier_;

 private:
  MockNewWindowDelegate new_window_delegate_;
};

TEST_F(WebsiteApprovalNotifierTest, ShowNotificationsForValidHosts) {
  std::string host1 = "www.google.com";
  std::string host2 = "images.google.com";
  OnNewWebsiteApproval(host1);
  OnNewWebsiteApproval(host2);
  // Expect both notifications to be shown (no overriding).
  EXPECT_TRUE(GetApprovalNotification(host1));
  EXPECT_TRUE(GetApprovalNotification(host2));
}

TEST_F(WebsiteApprovalNotifierTest, NoNotificationForDomainPattern) {
  std::string host = "*.google.*";
  OnNewWebsiteApproval(host);
  EXPECT_FALSE(GetApprovalNotification(host));
}

TEST_F(WebsiteApprovalNotifierTest, NoNotificationForInvalidHost) {
  std::string host = "google.com:12three";
  OnNewWebsiteApproval(host);
  EXPECT_FALSE(GetApprovalNotification(host));
}

TEST_F(WebsiteApprovalNotifierTest, MetricRecording) {
  base::UserActionTester user_action_tester;
  std::string host = "www.google.com";
  OnNewWebsiteApproval(host);
  const message_center::Notification* notification =
      GetApprovalNotification(host);
  ASSERT_TRUE(notification);
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "SupervisedUsers_RemoteWebApproval_NotificationShown"));
  notification->delegate()->Click(/*button_index=*/std::nullopt,
                                  /*reply=*/std::nullopt);
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "SupervisedUsers_RemoteWebApproval_NotificationClicked"));
}

TEST_F(WebsiteApprovalNotifierTest, UrlOpensInPrimaryBrowser) {
  base::UserActionTester user_action_tester;
  std::string host = "www.google.com";
  std::string expected_url = std::string("https://") + host + "/";
  OnNewWebsiteApproval(host);
  const message_center::Notification* notification =
      GetApprovalNotification(host);
  ASSERT_TRUE(notification);
  EXPECT_CALL(new_window_delegate(),
              OpenUrl(GURL(expected_url),
                      NewWindowDelegate::OpenUrlFrom::kUserInteraction,
                      NewWindowDelegate::Disposition::kNewForegroundTab));
  notification->delegate()->Click(/*button_index=*/std::nullopt,
                                  /*reply=*/std::nullopt);
}

}  // namespace ash
