// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <AppKit/AppKit.h>
#import <objc/runtime.h>
#include <unistd.h>

#include "base/bind.h"
#include "base/i18n/number_formatting.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/run_loop.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/notifications/notification_platform_bridge_mac.h"
#include "chrome/browser/notifications/notification_test_util.h"
#include "chrome/browser/notifications/stub_alert_dispatcher_mac.h"
#include "chrome/browser/notifications/stub_notification_center_mac.h"
#include "chrome/browser/ui/cocoa/notifications/notification_builder_mac.h"
#include "chrome/browser/ui/cocoa/notifications/notification_constants_mac.h"
#include "chrome/browser/ui/cocoa/notifications/notification_response_builder_mac.h"
#include "chrome/common/buildflags.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "url/gurl.h"

using message_center::Notification;

class NotificationPlatformBridgeMacTest : public BrowserWithTestWindowTest {
 public:
  NotificationPlatformBridgeMacTest() {}
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    notification_center_.reset([[StubNotificationCenter alloc] init]);
    alert_dispatcher_.reset([[StubAlertDispatcher alloc] init]);
  }

  void TearDown() override {
    [notification_center_ removeAllDeliveredNotifications];
    [alert_dispatcher_ closeAllNotifications];
    BrowserWithTestWindowTest::TearDown();
  }

 protected:
  static void StoreNotificationCount(int* out_notification_count,
                                     std::set<std::string> notifications,
                                     bool supports_synchronization) {
    DCHECK(out_notification_count);
    *out_notification_count = notifications.size();
  }

  std::unique_ptr<Notification> CreateBanner(const char* title,
                                             const char* subtitle,
                                             const char* origin,
                                             const char* button1,
                                             const char* button2) {
    return CreateNotification(title, subtitle, origin, button1, button2,
                              /*require_interaction=*/false,
                              /*show_settings_button=*/true);
  }

  std::unique_ptr<Notification> CreateAlert(const char* title,
                                            const char* subtitle,
                                            const char* origin,
                                            const char* button1,
                                            const char* button2) {
    return CreateNotification(title, subtitle, origin, button1, button2,
                              /*require_interaction=*/true,
                              /*show_settings_button=*/true);
  }

  std::unique_ptr<Notification> CreateNotification(const char* title,
                                                   const char* subtitle,
                                                   const char* origin,
                                                   const char* button1,
                                                   const char* button2,
                                                   bool require_interaction,
                                                   bool show_settings_button) {
    message_center::RichNotificationData optional_fields;
    if (button1) {
      optional_fields.buttons.push_back(
          message_center::ButtonInfo(base::UTF8ToUTF16(button1)));
      if (button2) {
        optional_fields.buttons.push_back(
            message_center::ButtonInfo(base::UTF8ToUTF16(button2)));
      }
    }
    if (show_settings_button) {
      optional_fields.settings_button_handler =
          message_center::SettingsButtonHandler::DELEGATE;
    }

    GURL url = GURL(origin);

    auto notification = std::make_unique<Notification>(
        message_center::NOTIFICATION_TYPE_SIMPLE, "id1",
        base::UTF8ToUTF16(title), base::UTF8ToUTF16(subtitle), gfx::Image(),
        base::UTF8ToUTF16("Notifier's Name"), url,
        message_center::NotifierId(url), optional_fields,
        new message_center::NotificationDelegate());
    if (require_interaction)
      notification->set_never_timeout(true);

    return notification;
  }

  NSUserNotificationCenter* notification_center() {
    return notification_center_.get();
  }

  StubAlertDispatcher* alert_dispatcher() { return alert_dispatcher_.get(); }

 private:
  base::scoped_nsobject<StubNotificationCenter> notification_center_;
  base::scoped_nsobject<StubAlertDispatcher> alert_dispatcher_;
};

TEST_F(NotificationPlatformBridgeMacTest, TestDisplayNoButtons) {
  std::unique_ptr<Notification> notification =
      CreateBanner("Title", "Context", "https://gmail.com", nullptr, nullptr);

  std::unique_ptr<NotificationPlatformBridgeMac> bridge(
      new NotificationPlatformBridgeMac(notification_center(),
                                        alert_dispatcher()));
  bridge->Display(NotificationHandler::Type::WEB_PERSISTENT, profile(),
                  *notification, nullptr);
  NSArray* notifications = [notification_center() deliveredNotifications];

  EXPECT_EQ(1u, [notifications count]);

  NSUserNotification* delivered_notification = [notifications objectAtIndex:0];
  EXPECT_NSEQ(@"Title", [delivered_notification title]);
  EXPECT_NSEQ(@"Context", [delivered_notification informativeText]);
  EXPECT_NSEQ(@"gmail.com", [delivered_notification subtitle]);
  EXPECT_NSEQ(@"Close", [delivered_notification otherButtonTitle]);
  EXPECT_NSEQ(@"Settings", [delivered_notification actionButtonTitle]);
}

TEST_F(NotificationPlatformBridgeMacTest, TestDisplayNoSettings) {
  std::unique_ptr<Notification> notification = CreateNotification(
      "Title", "Context", "https://gmail.com", nullptr, nullptr,
      /*require_interaction=*/false, /*show_settings_button=*/false);

  std::unique_ptr<NotificationPlatformBridgeMac> bridge(
      new NotificationPlatformBridgeMac(notification_center(),
                                        alert_dispatcher()));
  bridge->Display(NotificationHandler::Type::WEB_PERSISTENT, profile(),
                  *notification, nullptr);
  NSArray* notifications = [notification_center() deliveredNotifications];

  EXPECT_EQ(1u, [notifications count]);

  NSUserNotification* delivered_notification = [notifications objectAtIndex:0];
  EXPECT_NSEQ(@"Title", [delivered_notification title]);
  EXPECT_NSEQ(@"Context", [delivered_notification informativeText]);
  EXPECT_NSEQ(@"gmail.com", [delivered_notification subtitle]);
  EXPECT_NSEQ(@"Close", [delivered_notification otherButtonTitle]);
  EXPECT_FALSE([delivered_notification hasActionButton]);
}

TEST_F(NotificationPlatformBridgeMacTest, TestDisplayOneButton) {
  std::unique_ptr<Notification> notification = CreateBanner(
      "Title", "Context", "https://gmail.com", "Button 1", nullptr);

  std::unique_ptr<NotificationPlatformBridgeMac> bridge(
      new NotificationPlatformBridgeMac(notification_center(),
                                        alert_dispatcher()));
  bridge->Display(NotificationHandler::Type::WEB_PERSISTENT, profile(),
                  *notification, nullptr);

  NSArray* notifications = [notification_center() deliveredNotifications];
  EXPECT_EQ(1u, [notifications count]);
  NSUserNotification* delivered_notification = [notifications objectAtIndex:0];
  EXPECT_NSEQ(@"Title", [delivered_notification title]);
  EXPECT_NSEQ(@"Context", [delivered_notification informativeText]);
  EXPECT_NSEQ(@"gmail.com", [delivered_notification subtitle]);
  EXPECT_NSEQ(@"Close", [delivered_notification otherButtonTitle]);
  EXPECT_NSEQ(@"More", [delivered_notification actionButtonTitle]);
}

TEST_F(NotificationPlatformBridgeMacTest, TestDisplayProgress) {
  if (!NotificationPlatformBridgeMac::SupportsAlerts())
    return;

  std::unique_ptr<Notification> notification =
      CreateBanner("Title", "Context", "https://gmail.com", nullptr, nullptr);
  const int kSamplePercent = 10;

  notification->set_progress(kSamplePercent);
  notification->set_type(message_center::NOTIFICATION_TYPE_PROGRESS);

  std::unique_ptr<NotificationPlatformBridgeMac> bridge(
      new NotificationPlatformBridgeMac(notification_center(),
                                        alert_dispatcher()));
  bridge->Display(NotificationHandler::Type::WEB_PERSISTENT, profile(),
                  *notification, nullptr);

  // Progress notifications are considered alerts
  EXPECT_EQ(0u, [[notification_center() deliveredNotifications] count]);
  NSArray* displayedAlerts = [alert_dispatcher() alerts];
  ASSERT_EQ(1u, [displayedAlerts count]);

  NSDictionary* deliveredNotification = [displayedAlerts objectAtIndex:0];
  base::string16 expected =
      base::FormatPercent(kSamplePercent) + base::UTF8ToUTF16(" - Title");
  EXPECT_NSEQ(base::SysUTF16ToNSString(expected),
              [deliveredNotification objectForKey:@"title"]);
}

TEST_F(NotificationPlatformBridgeMacTest, TestCloseNotification) {
  std::unique_ptr<Notification> notification = CreateBanner(
      "Title", "Context", "https://gmail.com", "Button 1", nullptr);

  std::unique_ptr<NotificationPlatformBridgeMac> bridge(
      new NotificationPlatformBridgeMac(notification_center(),
                                        alert_dispatcher()));
  EXPECT_EQ(0u, [[notification_center() deliveredNotifications] count]);
  bridge->Display(NotificationHandler::Type::WEB_PERSISTENT, profile(),
                  *notification, nullptr);
  EXPECT_EQ(1u, [[notification_center() deliveredNotifications] count]);

  bridge->Close(profile(), "id1");
  EXPECT_EQ(0u, [[notification_center() deliveredNotifications] count]);
}

TEST_F(NotificationPlatformBridgeMacTest, TestGetDisplayed) {
  std::unique_ptr<Notification> notification = CreateBanner(
      "Title", "Context", "https://gmail.com", "Button 1", nullptr);
  std::unique_ptr<NotificationPlatformBridgeMac> bridge(
      new NotificationPlatformBridgeMac(notification_center(),
                                        alert_dispatcher()));
  EXPECT_EQ(0u, [[notification_center() deliveredNotifications] count]);
  bridge->Display(NotificationHandler::Type::WEB_PERSISTENT, profile(),
                  *notification, nullptr);
  EXPECT_EQ(1u, [[notification_center() deliveredNotifications] count]);

  int notification_count = -1;
  bridge->GetDisplayed(
      profile(), base::Bind(&StoreNotificationCount, &notification_count));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, notification_count);
}

TEST_F(NotificationPlatformBridgeMacTest, TestQuitRemovesNotifications) {
  std::unique_ptr<Notification> notification = CreateBanner(
      "Title", "Context", "https://gmail.com", "Button 1", nullptr);
  {
    std::unique_ptr<NotificationPlatformBridgeMac> bridge(
        new NotificationPlatformBridgeMac(notification_center(),
                                          alert_dispatcher()));
    EXPECT_EQ(0u, [[notification_center() deliveredNotifications] count]);
    bridge->Display(NotificationHandler::Type::WEB_PERSISTENT, profile(),
                    *notification, nullptr);
    EXPECT_EQ(1u, [[notification_center() deliveredNotifications] count]);
  }

  // The destructor of the bridge should close all notifications.
  EXPECT_EQ(0u, [[notification_center() deliveredNotifications] count]);
}

TEST_F(NotificationPlatformBridgeMacTest, TestDisplayAlert) {
  if (!NotificationPlatformBridgeMac::SupportsAlerts())
    return;

  std::unique_ptr<Notification> alert =
      CreateAlert("Title", "Context", "https://gmail.com", "Button 1", nullptr);
  std::unique_ptr<NotificationPlatformBridgeMac> bridge(
      new NotificationPlatformBridgeMac(notification_center(),
                                        alert_dispatcher()));
  bridge->Display(NotificationHandler::Type::WEB_PERSISTENT, profile(), *alert,
                  nullptr);
  EXPECT_EQ(0u, [[notification_center() deliveredNotifications] count]);
  EXPECT_EQ(1u, [[alert_dispatcher() alerts] count]);
}

TEST_F(NotificationPlatformBridgeMacTest, TestDisplayBannerAndAlert) {
  if (!NotificationPlatformBridgeMac::SupportsAlerts())
    return;

  std::unique_ptr<Notification> alert =
      CreateAlert("Title", "Context", "https://gmail.com", "Button 1", nullptr);
  std::unique_ptr<Notification> banner = CreateBanner(
      "Title", "Context", "https://gmail.com", "Button 1", nullptr);
  std::unique_ptr<NotificationPlatformBridgeMac> bridge(
      new NotificationPlatformBridgeMac(notification_center(),
                                        alert_dispatcher()));
  bridge->Display(NotificationHandler::Type::WEB_PERSISTENT, profile(),
                  Notification("notification_id1", *banner), nullptr);
  bridge->Display(NotificationHandler::Type::WEB_PERSISTENT, profile(),
                  Notification("notification_id2", *alert), nullptr);
  EXPECT_EQ(1u, [[notification_center() deliveredNotifications] count]);
  EXPECT_EQ(1u, [[alert_dispatcher() alerts] count]);
}

TEST_F(NotificationPlatformBridgeMacTest, TestCloseAlert) {
  if (!NotificationPlatformBridgeMac::SupportsAlerts())
    return;

  std::unique_ptr<Notification> alert =
      CreateAlert("Title", "Context", "https://gmail.com", "Button 1", nullptr);
  std::unique_ptr<NotificationPlatformBridgeMac> bridge(
      new NotificationPlatformBridgeMac(notification_center(),
                                        alert_dispatcher()));
  EXPECT_EQ(0u, [[alert_dispatcher() alerts] count]);
  bridge->Display(NotificationHandler::Type::WEB_PERSISTENT, profile(), *alert,
                  nullptr);
  EXPECT_EQ(1u, [[alert_dispatcher() alerts] count]);

  bridge->Close(profile(), "id1");
  EXPECT_EQ(0u, [[notification_center() deliveredNotifications] count]);
}

TEST_F(NotificationPlatformBridgeMacTest, TestQuitRemovesBannersAndAlerts) {
  if (!NotificationPlatformBridgeMac::SupportsAlerts())
    return;

  std::unique_ptr<Notification> notification = CreateBanner(
      "Title", "Context", "https://gmail.com", "Button 1", nullptr);
  std::unique_ptr<Notification> alert =
      CreateAlert("Title", "Context", "https://gmail.com", "Button 1", nullptr);
  {
    std::unique_ptr<NotificationPlatformBridgeMac> bridge(
        new NotificationPlatformBridgeMac(notification_center(),
                                          alert_dispatcher()));
    EXPECT_EQ(0u, [[notification_center() deliveredNotifications] count]);
    EXPECT_EQ(0u, [[alert_dispatcher() alerts] count]);
    bridge->Display(NotificationHandler::Type::WEB_PERSISTENT, profile(),
                    Notification("notification_id1", *notification), nullptr);
    bridge->Display(NotificationHandler::Type::WEB_PERSISTENT, profile(),
                    Notification("notification_id2", *alert), nullptr);
    EXPECT_EQ(1u, [[notification_center() deliveredNotifications] count]);
    EXPECT_EQ(1u, [[alert_dispatcher() alerts] count]);
  }

  // The destructor of the bridge should close all notifications.
  EXPECT_EQ(0u, [[notification_center() deliveredNotifications] count]);
  EXPECT_EQ(0u, [[alert_dispatcher() alerts] count]);
}

TEST_F(NotificationPlatformBridgeMacTest, TestDisplayETLDPlusOne) {
  std::unique_ptr<Notification> notification = CreateBanner(
      "Title", "Context", "https://overthelimit.hello.world.test.co.uk",
      "Button 1", nullptr);

  std::unique_ptr<NotificationPlatformBridgeMac> bridge(
      new NotificationPlatformBridgeMac(notification_center(),
                                        alert_dispatcher()));
  bridge->Display(NotificationHandler::Type::WEB_PERSISTENT, profile(),
                  Notification("notification_id1", *notification), nullptr);

  notification = CreateBanner("Title", "Context", "https://mail.appspot.com",
                              "Button 1", nullptr);
  bridge->Display(NotificationHandler::Type::WEB_PERSISTENT, profile(),
                  Notification("notification_id2", *notification), nullptr);

  notification = CreateBanner("Title", "Context", "https://tests.peter.sh",
                              "Button 1", nullptr);
  bridge->Display(NotificationHandler::Type::WEB_PERSISTENT, profile(),
                  Notification("notification_id3", *notification), nullptr);

  notification = CreateBanner(
      "Title", "Context",
      "https://somereallylongsubdomainthatactuallyisanaliasfortests.peter.sh/",
      "Button 1", nullptr);
  bridge->Display(NotificationHandler::Type::WEB_PERSISTENT, profile(),
                  Notification("notification_id4", *notification), nullptr);

  notification = CreateBanner("Title", "Context", "http://localhost:8080",
                              "Button 1", nullptr);
  bridge->Display(NotificationHandler::Type::WEB_PERSISTENT, profile(),
                  Notification("notification_id5", *notification), nullptr);

  notification = CreateBanner("Title", "Context", "https://93.186.186.172",
                              "Button 1", nullptr);
  bridge->Display(NotificationHandler::Type::WEB_PERSISTENT, profile(),
                  Notification("notification_id6", *notification), nullptr);

  NSArray* notifications = [notification_center() deliveredNotifications];
  EXPECT_EQ(6u, [notifications count]);
  NSUserNotification* delivered_notification = [notifications objectAtIndex:0];
  EXPECT_NSEQ(@"test.co.uk", [delivered_notification subtitle]);
  delivered_notification = [notifications objectAtIndex:1];
  EXPECT_NSEQ(@"mail.appspot.com", [delivered_notification subtitle]);
  delivered_notification = [notifications objectAtIndex:2];
  EXPECT_NSEQ(@"tests.peter.sh", [delivered_notification subtitle]);
  delivered_notification = [notifications objectAtIndex:3];
  EXPECT_NSEQ(@"peter.sh", [delivered_notification subtitle]);
  delivered_notification = [notifications objectAtIndex:4];
  EXPECT_NSEQ(@"localhost:8080", [delivered_notification subtitle]);
  delivered_notification = [notifications objectAtIndex:5];
  EXPECT_NSEQ(@"93.186.186.172", [delivered_notification subtitle]);
}
