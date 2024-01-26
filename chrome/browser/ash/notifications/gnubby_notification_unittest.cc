// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/notifications/gnubby_notification.h"

#include <memory>

#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/ash/components/dbus/gnubby/gnubby_client.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace ash {

class GnubbyNotificationTest : public BrowserWithTestWindowTest {
 public:
  GnubbyNotificationTest() {}
  ~GnubbyNotificationTest() override {}

  void SetUp() override {
    GnubbyClient::InitializeFake();
    ConciergeClient::InitializeFake(/*fake_cicerone_client=*/nullptr);
    BrowserWithTestWindowTest::SetUp();

    TestingBrowserProcess::GetGlobal()->SetSystemNotificationHelper(
        std::make_unique<SystemNotificationHelper>());
    tester_ = std::make_unique<NotificationDisplayServiceTester>(
        nullptr /* profile */);
    tester_->SetNotificationAddedClosure(base::BindRepeating(
        &GnubbyNotificationTest::OnNotificationAdded, base::Unretained(this)));
    gnubby_notification_ = std::make_unique<GnubbyNotification>();
    notification_count_ = 0;
  }

  std::optional<message_center::Notification> GetNotification() {
    return tester_->GetNotification("gnubby_notification");
  }

  void TearDown() override {
    gnubby_notification_.reset();
    tester_.reset();
    BrowserWithTestWindowTest::TearDown();
    ConciergeClient::Shutdown();
    GnubbyClient::Shutdown();
  }

  void OnNotificationAdded() { notification_count_++; }

 protected:
  std::unique_ptr<NotificationDisplayServiceTester> tester_;
  std::unique_ptr<GnubbyNotification> gnubby_notification_;
  int notification_count_ = 0;
};

TEST_F(GnubbyNotificationTest, OneNotificationsTest) {
  std::u16string expected_title =
      l10n_util::GetStringUTF16(IDS_GNUBBY_NOTIFICATION_TITLE);
  gnubby_notification_->ShowNotification();
  auto notification = GetNotification();
  ASSERT_TRUE(notification);
  EXPECT_EQ(expected_title, notification->title());
  EXPECT_EQ(1, notification_count_);
}

TEST_F(GnubbyNotificationTest, TwoNotificationsTest) {
  std::u16string expected_title =
      l10n_util::GetStringUTF16(IDS_GNUBBY_NOTIFICATION_TITLE);
  gnubby_notification_->ShowNotification();
  gnubby_notification_->DismissNotification();
  gnubby_notification_->ShowNotification();
  auto notification = GetNotification();
  ASSERT_TRUE(notification);
  EXPECT_EQ(expected_title, notification->title());
  EXPECT_EQ(2, notification_count_);
}

}  // namespace ash
