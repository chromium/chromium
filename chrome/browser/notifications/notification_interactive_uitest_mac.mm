// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <AppKit/AppKit.h>

#include "chrome/browser/notifications/notification_interactive_uitest_support.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#import "ui/base/test/windowed_nsnotification_observer.h"
#include "ui/message_center/message_center.h"

// Verify that Chrome becomes the active app if a notification opens a popup
// when clicked.
IN_PROC_BROWSER_TEST_F(NotificationsTest, TestPopupShouldActivateApp) {
  EXPECT_TRUE(embedded_test_server()->Start());

  AllowAllOrigins();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetTestPageURL()));

  EXPECT_TRUE(ui_test_utils::ShowAndFocusNativeWindow(
      browser()->window()->GetNativeWindow()));

  {
    WindowedNSNotificationObserver* observer =
        [[WindowedNSNotificationObserver alloc]
            initForNotification:NSApplicationDidHideNotification
                         object:NSApp];
    [NSApp hide:nil];
    [observer wait];
  }
  EXPECT_TRUE([NSApp isHidden]);

  WindowedNSNotificationObserver* observer =
      [[WindowedNSNotificationObserver alloc]
          initForNotification:NSApplicationDidUnhideNotification
                       object:NSApp];

  std::string result = CreateNotification(
      browser(), true, "", "", "", "",
      "window.open('', '', 'width=100,height=100').focus();");
  EXPECT_NE("-1", result);

  auto* message_center = message_center::MessageCenter::Get();
  message_center::Notification* notification =
      *message_center->GetVisibleNotifications().begin();

  message_center->ClickOnNotification(notification->id());
  [observer wait];

  EXPECT_FALSE([NSApp isHidden]);
}
