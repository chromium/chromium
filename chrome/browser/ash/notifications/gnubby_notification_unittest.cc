// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/notifications/gnubby_notification.h"

#include <memory>

#include "ash/public/cpp/message_center/oobe_notification_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/ash/components/dbus/gnubby/gnubby_client.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/test/message_center_waiter.h"

namespace ash {

class GnubbyNotificationTest : public BrowserWithTestWindowTest {
 public:
  GnubbyNotificationTest() = default;
  ~GnubbyNotificationTest() override = default;

  void SetUp() override {
    GnubbyClient::InitializeFake();
    ConciergeClient::InitializeFake(/*fake_cicerone_client=*/nullptr);
    BrowserWithTestWindowTest::SetUp();

    gnubby_notification_ = std::make_unique<GnubbyNotification>();
  }

  const message_center::Notification* GetNotification() {
    return message_center::MessageCenter::Get()->FindVisibleNotificationById(
        kOOBEGnubbyNotificationId);
  }

  void TearDown() override {
    gnubby_notification_.reset();
    BrowserWithTestWindowTest::TearDown();
    ConciergeClient::Shutdown();
    GnubbyClient::Shutdown();
  }

 protected:
  std::unique_ptr<GnubbyNotification> gnubby_notification_;
};

TEST_F(GnubbyNotificationTest, OneNotificationsTest) {
  std::u16string expected_title =
      l10n_util::GetStringUTF16(IDS_GNUBBY_NOTIFICATION_TITLE);
  message_center::MessageCenterWaiter waiter(kOOBEGnubbyNotificationId);
  gnubby_notification_->ShowNotification();
  waiter.WaitUntilAdded();
  const auto* notification = GetNotification();
  ASSERT_TRUE(notification);
  EXPECT_EQ(expected_title, notification->title());
}

TEST_F(GnubbyNotificationTest, TwoNotificationsTest) {
  std::u16string expected_title =
      l10n_util::GetStringUTF16(IDS_GNUBBY_NOTIFICATION_TITLE);

  // Show first notification.
  {
    message_center::MessageCenterWaiter waiter(kOOBEGnubbyNotificationId);
    gnubby_notification_->ShowNotification();
    waiter.WaitUntilAdded();
    ASSERT_TRUE(GetNotification());
  }

  // Dismiss it.
  gnubby_notification_->DismissNotification();
  ASSERT_FALSE(GetNotification());

  // Show second notification.
  {
    message_center::MessageCenterWaiter waiter(kOOBEGnubbyNotificationId);
    gnubby_notification_->ShowNotification();
    waiter.WaitUntilAdded();
    const auto* notification = GetNotification();
    ASSERT_TRUE(notification);
    EXPECT_EQ(expected_title, notification->title());
  }
}

}  // namespace ash
