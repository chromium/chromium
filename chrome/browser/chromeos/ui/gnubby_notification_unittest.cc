// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/ui/gnubby_notification.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_gnubby_client.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace chromeos {

class GnubbyNotificationTest : public BrowserWithTestWindowTest {
 public:
  GnubbyNotificationTest() {}
  ~GnubbyNotificationTest() override {}

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    DBusThreadManager::GetSetterForTesting()->SetGnubbyClient(
        std::unique_ptr<GnubbyClient>(new FakeGnubbyClient));

    TestingBrowserProcess::GetGlobal()->SetSystemNotificationHelper(
        std::make_unique<SystemNotificationHelper>());
    tester_ = std::make_unique<NotificationDisplayServiceTester>(
        nullptr /* profile */);
    tester_->SetNotificationAddedClosure(base::BindRepeating(
        &GnubbyNotificationTest::OnNotificationAdded, base::Unretained(this)));
    gnubby_notification_.reset(new GnubbyNotification());
    notification_count_ = 0;
  }

  base::Optional<message_center::Notification> GetNotification() {
    return tester_->GetNotification("gnubby_notification");
  }

  void TearDown() override {
    gnubby_notification_.reset();
    tester_.reset();
    DBusThreadManager::GetSetterForTesting()->SetGnubbyClient(nullptr);
    BrowserWithTestWindowTest::TearDown();
  }

  void OnNotificationAdded() { notification_count_++; }

 protected:
  std::unique_ptr<NotificationDisplayServiceTester> tester_;
  std::unique_ptr<GnubbyNotification> gnubby_notification_;
  int notification_count_ = 0;
};

TEST_F(GnubbyNotificationTest, OneNotificationsTest) {
  base::string16 expected_title =
      l10n_util::GetStringUTF16(IDS_GNUBBY_NOTIFICATION_TITLE);
  gnubby_notification_->ShowNotification();
  auto notification = GetNotification();
  ASSERT_TRUE(notification);
  EXPECT_EQ(expected_title, notification->title());
  EXPECT_EQ(1, notification_count_);
}

TEST_F(GnubbyNotificationTest, TwoNotificationsTest) {
  base::string16 expected_title =
      l10n_util::GetStringUTF16(IDS_GNUBBY_NOTIFICATION_TITLE);
  gnubby_notification_->ShowNotification();
  gnubby_notification_->DismissNotification();
  gnubby_notification_->ShowNotification();
  auto notification = GetNotification();
  ASSERT_TRUE(notification);
  EXPECT_EQ(expected_title, notification->title());
  EXPECT_EQ(2, notification_count_);
}

}  // namespace chromeos
