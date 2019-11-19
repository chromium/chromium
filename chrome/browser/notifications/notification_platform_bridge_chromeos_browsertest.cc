// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_platform_bridge_chromeos.h"

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

class NotificationPlatformBridgeChromeOsBrowserTest
    : public InProcessBrowserTest,
      public message_center::NotificationObserver {
 public:
  NotificationPlatformBridgeChromeOsBrowserTest() = default;
  ~NotificationPlatformBridgeChromeOsBrowserTest() override {
    EXPECT_EQ(expected_close_count_, close_count_);
  }

  // message_center::NotificationObserver
  void Close(bool by_user) override { ++close_count_; }

 protected:
  int expected_close_count_ = 0;
  int close_count_ = 0;

  base::WeakPtrFactory<NotificationPlatformBridgeChromeOsBrowserTest>
      weak_ptr_factory_{this};

 private:
  DISALLOW_COPY_AND_ASSIGN(NotificationPlatformBridgeChromeOsBrowserTest);
};

// Tests that a notification delegate is informed of the notification closing
// when it's closed due to shutdown.
IN_PROC_BROWSER_TEST_F(NotificationPlatformBridgeChromeOsBrowserTest,
                       Shutdown) {
  message_center::Notification notification(
      message_center::NOTIFICATION_TYPE_SIMPLE, "notification-id",
      base::string16(), base::string16(), gfx::Image(), base::string16(),
      GURL(), message_center::NotifierId(),
      message_center::RichNotificationData(),
      base::MakeRefCounted<message_center::ThunkNotificationDelegate>(
          weak_ptr_factory_.GetWeakPtr()));

  NotificationDisplayService::GetForProfile(browser()->profile())
      ->Display(NotificationHandler::Type::TRANSIENT, notification,
                /*metadata=*/nullptr);
  expected_close_count_ = 1;
}
