// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/notifications/multi_capture_notification.h"

#include "ash/shell.h"
#include "base/bind.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/notifications/system_notification_helper.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/message_center/public/cpp/notification.h"
#include "url/origin.h"

namespace ash {

class MultiCaptureNotificationTest : public BrowserWithTestWindowTest {
 public:
  MultiCaptureNotificationTest() = default;
  ~MultiCaptureNotificationTest() override = default;

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    UserDataAuthClient::InitializeFake();

    auto user_manager = std::make_unique<user_manager::FakeUserManager>();
    user_manager_ = user_manager.get();
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(user_manager));

    TestingBrowserProcess::GetGlobal()->SetSystemNotificationHelper(
        std::make_unique<SystemNotificationHelper>());
    tester_ = std::make_unique<NotificationDisplayServiceTester>(
        /*profile=*/nullptr);
    tester_->SetNotificationAddedClosure(
        base::BindRepeating(&MultiCaptureNotificationTest::OnNotificationAdded,
                            base::Unretained(this)));
    multi_capture_notification_ = std::make_unique<MultiCaptureNotification>();
    notification_count_ = 0u;
  }

  void TearDown() override {
    multi_capture_notification_.reset();
    UserDataAuthClient::Shutdown();
    BrowserWithTestWindowTest::TearDown();
  }

  absl::optional<message_center::Notification> GetNotification() {
    return tester_->GetNotification("multi_capture");
  }

  void OnNotificationAdded() { notification_count_++; }

 protected:
  user_manager::FakeUserManager* user_manager_ = nullptr;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
  std::unique_ptr<NotificationDisplayServiceTester> tester_;
  std::unique_ptr<MultiCaptureNotification> multi_capture_notification_;
  unsigned int notification_count_;
};

TEST_F(MultiCaptureNotificationTest, NotificationTriggered) {
  multi_capture_notification_->MultiCaptureStarted(
      /*label=*/"test_label", /*origin=*/url::Origin::CreateFromNormalizedTuple(
          /*scheme=*/"https", /*host=*/"example.com", /*port=*/443));
  absl::optional<message_center::Notification> notification = GetNotification();
  ASSERT_TRUE(notification);
  EXPECT_EQ(u"", notification->title());
  EXPECT_EQ(
      u"Your system administrator has allowed example.com to record your "
      u"screen",
      notification->message());
  EXPECT_EQ(1u, notification_count_);
}

}  // namespace ash
