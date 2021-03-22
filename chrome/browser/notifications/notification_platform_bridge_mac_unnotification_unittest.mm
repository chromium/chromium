// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UserNotifications/UserNotifications.h>

#include "base/bind.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/process/process_handle.h"
#include "base/run_loop.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/notifications/notification_platform_bridge_mac_unnotification.h"
#include "chrome/browser/notifications/notification_platform_bridge_mac_utils.h"
#include "chrome/browser/notifications/stub_alert_dispatcher_mac.h"
#include "chrome/browser/notifications/unnotification_metrics.h"
#include "chrome/browser/ui/cocoa/notifications/unnotification_builder_mac.h"
#include "chrome/browser/ui/cocoa/notifications/unnotification_response_builder_mac.h"
#include "chrome/services/mac_notifications/public/cpp/notification_constants_mac.h"
#include "chrome/services/mac_notifications/public/cpp/notification_test_utils_mac.h"
#include "chrome/services/mac_notifications/public/cpp/notification_utils_mac.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "ui/message_center/public/cpp/notification.h"
#include "url/gurl.h"

using message_center::Notification;

class UNNotificationPlatformBridgeMacTest : public testing::Test {
 public:
  UNNotificationPlatformBridgeMacTest()
      : manager_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override {
    ASSERT_TRUE(manager_.SetUp());
    profile_ = manager_.CreateTestingProfile("Moe");
    display_service_tester_ =
        std::make_unique<NotificationDisplayServiceTester>(profile_);
    alert_dispatcher_.reset([[StubAlertDispatcher alloc] init]);
    if (@available(macOS 10.14, *)) {
      center_.reset([[FakeUNUserNotificationCenter alloc] init]);
      [[center_ settings] setAlertStyle:UNAlertStyleBanner];
      [[center_ settings]
          setAuthorizationStatus:UNAuthorizationStatusAuthorized];
      bridge_ = std::make_unique<NotificationPlatformBridgeMacUNNotification>(
          static_cast<UNUserNotificationCenter*>(center_.get()),
          alert_dispatcher_.get());
    }
  }

 protected:
  Notification CreateNotification(const std::string& notificationId = "id1") {
    GURL url("https://gmail.com");

    Notification notification(
        message_center::NOTIFICATION_TYPE_SIMPLE, notificationId, u"Title",
        u"Context", gfx::Image(), u"Notifier's Name", url,
        message_center::NotifierId(url), message_center::RichNotificationData(),
        base::MakeRefCounted<message_center::NotificationDelegate>());

    return notification;
  }

  Notification CreateAlert(const std::string& notificationId = "id1") {
    Notification notification = CreateNotification(notificationId);
    notification.set_never_timeout(true);
    return notification;
  }

  std::set<std::string> GetDisplayedSync() API_AVAILABLE(macosx(10.14)) {
    base::RunLoop run_loop;
    std::set<std::string> displayed;
    bridge_->GetDisplayed(profile_, base::BindLambdaForTesting(
                                        [&](std::set<std::string> notifications,
                                            bool supports_synchronization) {
                                          displayed = std::move(notifications);
                                          run_loop.Quit();
                                        }));
    run_loop.Run();
    return displayed;
  }

  base::scoped_nsobject<StubAlertDispatcher> alert_dispatcher_;
  API_AVAILABLE(macosx(10.14))
  base::scoped_nsobject<FakeUNUserNotificationCenter> center_;
  API_AVAILABLE(macosx(10.14))
  std::unique_ptr<NotificationPlatformBridgeMacUNNotification> bridge_;
  TestingProfile* profile_ = nullptr;
  base::HistogramTester histogram_tester_;
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingProfileManager manager_;
  std::unique_ptr<NotificationDisplayServiceTester> display_service_tester_;
};

class UNNotificationPlatformBridgeMacPermissionStatusTest
    : public UNNotificationPlatformBridgeMacTest,
      public testing::WithParamInterface<UNNotificationPermissionStatus> {};

class UNNotificationPlatformBridgeMacBannerStyleTest
    : public UNNotificationPlatformBridgeMacTest,
      public testing::WithParamInterface<UNNotificationStyle> {};

TEST_F(UNNotificationPlatformBridgeMacTest, TestDisplay) {
  if (@available(macOS 10.14, *)) {
    base::HistogramTester histogram_tester;
    Notification notification = CreateNotification();

    bridge_->Display(NotificationHandler::Type::WEB_PERSISTENT, profile_,
                     notification, nullptr);

    [center_ getDeliveredNotificationsWithCompletionHandler:^(
                 NSArray<UNNotification*>* _Nonnull notifications) {
      ASSERT_EQ(1u, [notifications count]);
      UNNotification* delivered_notification = [notifications objectAtIndex:0];
      UNNotificationContent* delivered_content =
          [[delivered_notification request] content];
      EXPECT_NSEQ(@"Title", [delivered_content title]);
      EXPECT_NSEQ(@"Context", [delivered_content body]);
      EXPECT_NSEQ(@"gmail.com", [delivered_content subtitle]);
    }];

    [center_ getNotificationCategoriesWithCompletionHandler:^(
                 NSSet<UNNotificationCategory*>* categories) {
      EXPECT_EQ(1u, [categories count]);
    }];

    histogram_tester.ExpectUniqueSample("Notifications.macOS.Delivered.Banner",
                                        /*sample=*/true, /*expected_count=*/1);
  }
}

TEST_F(UNNotificationPlatformBridgeMacTest, TestDisplayAlert) {
  if (@available(macOS 10.14, *)) {
    base::HistogramTester histogram_tester;
    Notification alert = CreateAlert();
    // Some OS versions don't support alerts.
    if (!IsAlertNotificationMac(alert))
      return;

    bridge_->Display(NotificationHandler::Type::WEB_PERSISTENT, profile_, alert,
                     /*metadata=*/nullptr);

    // Expect the alert to be shown via |alert_dispatcher_| and not |center_|.
    [center_ getDeliveredNotificationsWithCompletionHandler:^(
                 NSArray<UNNotification*>* _Nonnull notifications) {
      ASSERT_EQ(0u, [notifications count]);
    }];
    NSArray* displayed_alerts = [alert_dispatcher_ alerts];
    ASSERT_EQ(1u, [displayed_alerts count]);

    // Verify alert content.
    NSDictionary* delivered_alert = [displayed_alerts objectAtIndex:0];
    NSString* title = [delivered_alert
        objectForKey:notification_constants::kNotificationTitle];
    NSString* informative_text = [delivered_alert
        objectForKey:notification_constants::kNotificationInformativeText];
    NSString* subtitle = [delivered_alert
        objectForKey:notification_constants::kNotificationSubTitle];
    NSString* identifier = [delivered_alert
        objectForKey:notification_constants::kNotificationIdentifier];
    EXPECT_NSEQ(@"Title", title);
    EXPECT_NSEQ(@"Context", informative_text);
    EXPECT_NSEQ(@"gmail.com", subtitle);
    EXPECT_NSEQ(@"r|Moe|id1", identifier);

    histogram_tester.ExpectUniqueSample("Notifications.macOS.Delivered.Alert",
                                        /*sample=*/true, /*expected_count=*/1);
  }
}

TEST_F(UNNotificationPlatformBridgeMacTest, TestDisplayMultipleProfiles) {
  if (@available(macOS 10.14, *)) {
    TestingProfile* profile_1 = manager_.CreateTestingProfile("P1");
    TestingProfile* profile_2 = manager_.CreateTestingProfile("P2");

    // Show two notifications with the same id from different profiles.
    bridge_->Display(NotificationHandler::Type::WEB_PERSISTENT, profile_1,
                     CreateNotification("id"), /*metadata=*/nullptr);
    bridge_->Display(NotificationHandler::Type::WEB_PERSISTENT, profile_2,
                     CreateNotification("id"), /*metadata=*/nullptr);

    [center_ getDeliveredNotificationsWithCompletionHandler:^(
                 NSArray<UNNotification*>* _Nonnull notifications) {
      ASSERT_EQ(2u, [notifications count]);
    }];
  }
}

TEST_F(UNNotificationPlatformBridgeMacTest, TestIncognitoProfile) {
  if (@available(macOS 10.14, *)) {
    Notification notification = CreateNotification();

    TestingProfile::Builder profile_builder;
    profile_builder.SetPath(profile_->GetPath());
    profile_builder.SetProfileName(profile_->GetProfileUserName());
    Profile* incogito_profile = profile_builder.BuildIncognito(profile_);

    // Show two notifications with the same id from different profiles.
    bridge_->Display(NotificationHandler::Type::WEB_PERSISTENT, profile_,
                     notification, /*metadata=*/nullptr);
    bridge_->Display(NotificationHandler::Type::WEB_PERSISTENT,
                     incogito_profile, notification,
                     /*metadata=*/nullptr);
    [center_ getDeliveredNotificationsWithCompletionHandler:^(
                 NSArray<UNNotification*>* _Nonnull notifications) {
      EXPECT_EQ(2u, [notifications count]);
    }];

    // Close the one for the incognito profile.
    bridge_->Close(incogito_profile, "id1");
    // Close runs async code that we can't observe, make sure all tasks run.
    base::RunLoop().RunUntilIdle();

    __block UNNotification* remaining = nullptr;
    [center_ getDeliveredNotificationsWithCompletionHandler:^(
                 NSArray<UNNotification*>* _Nonnull notifications) {
      ASSERT_EQ(1u, [notifications count]);
      remaining = [notifications objectAtIndex:0];
    }];

    // Expect that the remaining notification is for the regular profile.
    EXPECT_EQ(false,
              [[[[[remaining request] content] userInfo]
                  objectForKey:notification_constants::kNotificationIncognito]
                  boolValue]);

    // Close the one for the regular profile.
    bridge_->Close(profile_, "id1");
    // Close runs async code that we can't observe, make sure all tasks run.
    base::RunLoop().RunUntilIdle();

    [center_ getDeliveredNotificationsWithCompletionHandler:^(
                 NSArray<UNNotification*>* _Nonnull notifications) {
      EXPECT_EQ(0u, [notifications count]);
    }];
  }
}

TEST_F(UNNotificationPlatformBridgeMacTest, TestNotificationHasIcon) {
  if (@available(macOS 10.14, *)) {
    Notification notification = CreateNotification();

    SkBitmap icon;
    icon.allocN32Pixels(64, 64);
    icon.eraseARGB(255, 100, 150, 200);
    notification.set_icon(gfx::Image::CreateFrom1xBitmap(icon));

    bridge_->Display(NotificationHandler::Type::WEB_PERSISTENT, profile_,
                     notification, nullptr);

    [center_ getDeliveredNotificationsWithCompletionHandler:^(
                 NSArray<UNNotification*>* _Nonnull notifications) {
      ASSERT_EQ(1u, [notifications count]);
      UNNotification* delivered_notification = [notifications objectAtIndex:0];
      UNNotificationContent* delivered_content =
          [[delivered_notification request] content];
      ASSERT_EQ(1u, [[delivered_content attachments] count]);
      EXPECT_NSEQ(@"id1", [[[delivered_content attachments] objectAtIndex:0]
                              identifier]);
    }];
  }
}

TEST_F(UNNotificationPlatformBridgeMacTest, TestNotificationNoIcon) {
  if (@available(macOS 10.14, *)) {
    Notification notification = CreateNotification();

    bridge_->Display(NotificationHandler::Type::WEB_PERSISTENT, profile_,
                     notification, nullptr);

    [center_ getDeliveredNotificationsWithCompletionHandler:^(
                 NSArray<UNNotification*>* _Nonnull notifications) {
      ASSERT_EQ(1u, [notifications count]);
      UNNotification* delivered_notification = [notifications objectAtIndex:0];
      UNNotificationContent* delivered_content =
          [[delivered_notification request] content];
      EXPECT_EQ(0u, [[delivered_content attachments] count]);
    }];
  }
}

TEST_F(UNNotificationPlatformBridgeMacTest, TestCloseNotification) {
  if (@available(macOS 10.14, *)) {
    Notification notification = CreateNotification();

    [center_ getDeliveredNotificationsWithCompletionHandler:^(
                 NSArray<UNNotification*>* _Nonnull notifications) {
      EXPECT_EQ(0u, [notifications count]);
    }];

    bridge_->Display(NotificationHandler::Type::WEB_PERSISTENT, profile_,
                     notification, nullptr);

    [center_ getDeliveredNotificationsWithCompletionHandler:^(
                 NSArray<UNNotification*>* _Nonnull notifications) {
      EXPECT_EQ(1u, [notifications count]);
    }];

    bridge_->Close(profile_, "id1");
    // RunLoop is used here to ensure that Close has finished executing before
    // the notification count is checked below. Since Close executes
    // asynchronous calls the order is not guaranteed by nature.
    base::RunLoop().RunUntilIdle();
    [center_ getDeliveredNotificationsWithCompletionHandler:^(
                 NSArray<UNNotification*>* _Nonnull notifications) {
      EXPECT_EQ(0u, [notifications count]);
    }];
  }
}

TEST_F(UNNotificationPlatformBridgeMacTest, TestCloseAlert) {
  if (@available(macOS 10.14, *)) {
    Notification alert = CreateAlert("id1");
    bridge_->Display(NotificationHandler::Type::WEB_PERSISTENT, profile_, alert,
                     /*metadata=*/nullptr);
    ASSERT_EQ(1u, [[alert_dispatcher_ alerts] count]);

    bridge_->Close(profile_, "id1");
    // RunLoop is used here to ensure that Close has finished executing before
    // the notification count is checked below. Since Close executes
    // asynchronous calls the order is not guaranteed by nature.
    base::RunLoop().RunUntilIdle();
    ASSERT_EQ(0u, [[alert_dispatcher_ alerts] count]);
  }
}

TEST_F(UNNotificationPlatformBridgeMacTest, TestGetDisplayed) {
  if (@available(macOS 10.14, *)) {
    Notification notification = CreateNotification();

    [center_ getDeliveredNotificationsWithCompletionHandler:^(
                 NSArray<UNNotification*>* _Nonnull notifications) {
      EXPECT_EQ(0u, [notifications count]);
    }];

    bridge_->Display(NotificationHandler::Type::WEB_PERSISTENT, profile_,
                     notification, nullptr);

    [center_ getDeliveredNotificationsWithCompletionHandler:^(
                 NSArray<UNNotification*>* _Nonnull notifications) {
      EXPECT_EQ(1u, [notifications count]);
    }];

    EXPECT_EQ(1u, GetDisplayedSync().size());
  }
}

TEST_F(UNNotificationPlatformBridgeMacTest,
       TestGetDisplayedMultipleNotifications) {
  if (@available(macOS 10.14, *)) {
    // Setup 2 banners and 1 alert for the correct profile.
    bridge_->Display(NotificationHandler::Type::WEB_PERSISTENT, profile_,
                     CreateNotification("id1"), /*metadata=*/nullptr);
    bridge_->Display(NotificationHandler::Type::WEB_PERSISTENT, profile_,
                     CreateNotification("id2"), /*metadata=*/nullptr);
    bridge_->Display(NotificationHandler::Type::WEB_PERSISTENT, profile_,
                     CreateAlert("id3"), /*metadata=*/nullptr);

    // Add some more notifications for other profiles that shouldn't show up.
    TestingProfile::Builder profile_builder;
    profile_builder.SetPath(profile_->GetPath());
    profile_builder.SetProfileName(profile_->GetProfileUserName());
    Profile* incogito_profile = profile_builder.BuildIncognito(profile_);
    bridge_->Display(NotificationHandler::Type::WEB_PERSISTENT,
                     incogito_profile, CreateNotification("id1"),
                     /*metadata=*/nullptr);
    bridge_->Display(NotificationHandler::Type::WEB_PERSISTENT,
                     incogito_profile, CreateAlert("id99"),
                     /*metadata=*/nullptr);
    TestingProfile* other_profile = manager_.CreateTestingProfile("Other");
    bridge_->Display(NotificationHandler::Type::WEB_PERSISTENT, other_profile,
                     CreateNotification("id2"), /*metadata=*/nullptr);

    EXPECT_EQ(3u, GetDisplayedSync().size());

    size_t expected_alerts = IsAlertNotificationMac(CreateAlert()) ? 2 : 0;
    [center_ getDeliveredNotificationsWithCompletionHandler:^(
                 NSArray<UNNotification*>* _Nonnull notifications) {
      EXPECT_EQ(6u - expected_alerts, [notifications count]);
    }];
    ASSERT_EQ(expected_alerts, [[alert_dispatcher_ alerts] count]);
  }
}

TEST_F(UNNotificationPlatformBridgeMacTest, TestQuitRemovesNotifications) {
  if (@available(macOS 10.14, *)) {
    // Setup one banner and one alert notification.
    bridge_->Display(NotificationHandler::Type::WEB_PERSISTENT, profile_,
                     CreateNotification(), nullptr);
    [alert_dispatcher_ dispatchNotification:@{}];

    // Check that both notifications are present.
    [center_ getDeliveredNotificationsWithCompletionHandler:^(
                 NSArray<UNNotification*>* _Nonnull notifications) {
      EXPECT_EQ(1u, [notifications count]);
    }];
    EXPECT_EQ(1u, [[alert_dispatcher_ alerts] count]);

    // Destroying the bridge should close all notifications.
    bridge_.reset();

    // Check that all notifications are closed.
    [center_ getDeliveredNotificationsWithCompletionHandler:^(
                 NSArray<UNNotification*>* _Nonnull notifications) {
      EXPECT_EQ(0u, [notifications count]);
    }];
    EXPECT_EQ(0u, [[alert_dispatcher_ alerts] count]);
  }
}

TEST_F(UNNotificationPlatformBridgeMacTest,
       TestProfileShutdownRemovesNotifications) {
  if (@available(macOS 10.14, *)) {
    Notification notification = CreateAlert();

    TestingProfile::Builder profile_builder;
    profile_builder.SetPath(profile_->GetPath());
    profile_builder.SetProfileName(profile_->GetProfileUserName());
    Profile* incognito_profile = profile_builder.BuildIncognito(profile_);

    // Show two notifications with the same id from different profiles.
    bridge_->Display(NotificationHandler::Type::WEB_PERSISTENT, profile_,
                     notification, /*metadata=*/nullptr);
    bridge_->Display(NotificationHandler::Type::WEB_PERSISTENT,
                     incognito_profile, notification,
                     /*metadata=*/nullptr);
    ASSERT_EQ(2u, [[alert_dispatcher_ alerts] count]);

    // Start shutdown of the incognito profile.
    bridge_->DisplayServiceShutDown(incognito_profile);
    // This runs async code that we can't observe, make sure all tasks run.
    base::RunLoop().RunUntilIdle();

    NSArray* displayed_alerts = [alert_dispatcher_ alerts];
    ASSERT_EQ(1u, [displayed_alerts count]);
    NSDictionary* remaining = [displayed_alerts objectAtIndex:0];

    // Expect that the remaining notification is for the regular profile.
    EXPECT_FALSE(
        [[remaining objectForKey:notification_constants::kNotificationIncognito]
            boolValue]);
  }
}

// Regression test for crbug.com/1182795
TEST_F(UNNotificationPlatformBridgeMacTest, TestNullProfileShutdown) {
  if (@available(macOS 10.14, *)) {
    // Emulate shutdown of the null profile.
    bridge_->DisplayServiceShutDown(/*profile=*/nullptr);
  }
}

TEST_F(UNNotificationPlatformBridgeMacTest, TestNotificationNoButtons) {
  if (@available(macOS 10.14, *)) {
    Notification notification = CreateNotification();

    notification.set_settings_button_handler(
        message_center::SettingsButtonHandler::DELEGATE);

    bridge_->Display(NotificationHandler::Type::WEB_PERSISTENT, profile_,
                     notification, nullptr);

    [center_ getNotificationCategoriesWithCompletionHandler:^(
                 NSSet<UNNotificationCategory*>* categories) {
      ASSERT_EQ(1u, [categories count]);
      UNNotificationCategory* category = [categories anyObject];
      // If this selector from the private API is available the close button
      // will be set in alernateAction, and the other buttons will be set in
      // actions. Otherwise, all buttons will be setting in actions which causes
      // the total count to differ by one. On macOS 11+ we don't need to add the
      // close button as there will always be one in the top left corner.
      if (base::mac::IsAtLeastOS11()) {
        ASSERT_EQ(1ul, [[category actions] count]);
        EXPECT_NSEQ(@"Settings", [[[category actions] lastObject] title]);
      } else if ([category respondsToSelector:@selector(alternateAction)]) {
        ASSERT_EQ(1ul, [[category actions] count]);
        EXPECT_NSEQ(@"Close",
                    [[category valueForKey:@"_alternateAction"] title]);
        EXPECT_NSEQ(@"Settings", [[[category actions] lastObject] title]);
      } else {
        ASSERT_EQ(2ul, [[category actions] count]);
        EXPECT_NSEQ(@"Close", [[[category actions] lastObject] title]);
        EXPECT_NSEQ(@"Settings", [[[category actions] firstObject] title]);
      }
    }];
  }
}

TEST_F(UNNotificationPlatformBridgeMacTest, TestNotificationNoSettingsButton) {
  if (@available(macOS 10.14, *)) {
    Notification notification = CreateNotification();

    bridge_->Display(NotificationHandler::Type::WEB_PERSISTENT, profile_,
                     notification, nullptr);

    [center_ getNotificationCategoriesWithCompletionHandler:^(
                 NSSet<UNNotificationCategory*>* categories) {
      ASSERT_EQ(1u, [categories count]);
      UNNotificationCategory* category = [categories anyObject];

      if ([category respondsToSelector:@selector(alternateAction)])
        EXPECT_EQ(0ul, [[category actions] count]);
      else
        EXPECT_EQ(1ul, [[category actions] count]);
    }];
  }
}

TEST_F(UNNotificationPlatformBridgeMacTest, TestNotificationWithButtons) {
  if (@available(macOS 10.14, *)) {
    Notification notification = CreateNotification();

    notification.set_settings_button_handler(
        message_center::SettingsButtonHandler::DELEGATE);
    std::vector<message_center::ButtonInfo> buttons = {
        message_center::ButtonInfo(u"Button 1"),
        message_center::ButtonInfo(u"Button 2")};
    notification.set_buttons(buttons);

    bridge_->Display(NotificationHandler::Type::WEB_PERSISTENT, profile_,
                     notification, nullptr);

    [center_ getNotificationCategoriesWithCompletionHandler:^(
                 NSSet<UNNotificationCategory*>* categories) {
      ASSERT_EQ(1u, [categories count]);
      UNNotificationCategory* category = [categories anyObject];

      if ([category respondsToSelector:@selector(alternateAction)]) {
        EXPECT_NSEQ(@"Button 1", [[category actions][0] title]);
        EXPECT_NSEQ(@"Button 2", [[category actions][1] title]);
        EXPECT_EQ(3ul, [[category actions] count]);
      } else {
        EXPECT_NSEQ(@"Button 1", [[category actions][1] title]);
        EXPECT_NSEQ(@"Button 2", [[category actions][2] title]);
        EXPECT_EQ(4ul, [[category actions] count]);
      }
    }];
  }
}

TEST_F(UNNotificationPlatformBridgeMacTest,
       TestNotificationCategoryIdentifier) {
  if (@available(macOS 10.14, *)) {
    Notification notification = CreateNotification();

    bridge_->Display(NotificationHandler::Type::WEB_PERSISTENT, profile_,
                     notification, nullptr);

    ASSERT_EQ(1u, [[center_ categories] count]);
    NSString* category_id = [[[center_ categories] anyObject] identifier];

    ASSERT_EQ(1u, [[center_ notifications] count]);
    EXPECT_NSEQ(category_id, [[[[center_ notifications][0] request] content]
                                 categoryIdentifier]);
  }
}

TEST_F(UNNotificationPlatformBridgeMacTest, TestCloseRemovesCategory) {
  if (@available(macOS 10.14, *)) {
    Notification first_notification = CreateNotification();

    bridge_->Display(NotificationHandler::Type::WEB_PERSISTENT, profile_,
                     first_notification, nullptr);

    ASSERT_EQ(1u, [[center_ categories] count]);
    NSString* category_id = [[[center_ categories] anyObject] identifier];

    bridge_->Close(profile_, "id1");
    // RunLoop is used here to ensure that Close has finished executing before
    // the notification count is checked below. Since Close executes
    // asynchronous calls the order is not guaranteed by nature.
    base::RunLoop().RunUntilIdle();

    // Categories get updated during the next display call, so we call display
    // to make sure that the category has been removed.
    Notification second_notification = CreateNotification("id2");
    bridge_->Display(NotificationHandler::Type::WEB_PERSISTENT, profile_,
                     second_notification, nullptr);

    // Expect that there is now a new category.
    ASSERT_EQ(1u, [[center_ categories] count]);
    EXPECT_NSNE(category_id, [[[center_ categories] anyObject] identifier]);
  }
}

TEST_F(UNNotificationPlatformBridgeMacTest, TestSynchronizeNotifications) {
  if (@available(macOS 10.14, *)) {
    base::HistogramTester histogram_tester;
    Notification banner1 = CreateNotification("banner1");
    Notification banner2 = CreateNotification("banner2");
    Notification alert1 = CreateAlert("alert1");
    Notification alert2 = CreateAlert("alert2");

    bridge_->Display(NotificationHandler::Type::WEB_PERSISTENT, profile_,
                     banner1, /*metadata=*/nullptr);
    bridge_->Display(NotificationHandler::Type::WEB_PERSISTENT, profile_,
                     banner2, /*metadata=*/nullptr);
    bridge_->Display(NotificationHandler::Type::WEB_PERSISTENT, profile_,
                     alert1, /*metadata=*/nullptr);
    bridge_->Display(NotificationHandler::Type::WEB_PERSISTENT, profile_,
                     alert2, /*metadata=*/nullptr);

    EXPECT_EQ(4u, GetDisplayedSync().size());

    // Remove two notifications without notifying the |brigde_|.
    std::string profile_id = NotificationPlatformBridge::GetProfileId(profile_);
    [center_ removeDeliveredNotificationsWithIdentifiers:@[
      base::SysUTF8ToNSString(DeriveMacNotificationId(
          profile_->IsOffTheRecord(), profile_id, "banner1"))
    ]];
    [alert_dispatcher_
        closeNotificationWithId:@"alert2"
                      profileId:base::SysUTF8ToNSString(profile_id)
                      incognito:profile_->IsOffTheRecord()];

    // Let some time pass but not enough to trigger synchronization.
    task_environment_.FastForwardBy(base::TimeDelta::FromMinutes(5));

    // Let more time pass to trigger synchronization and two close events.
    base::MockCallback<
        StubNotificationDisplayService::ProcessNotificationOperationCallback>
        operation_callback;
    EXPECT_CALL(operation_callback, Run).Times(2);
    display_service_tester_->SetProcessNotificationOperationDelegate(
        operation_callback.Get());
    task_environment_.FastForwardBy(base::TimeDelta::FromMinutes(5));

    histogram_tester.ExpectUniqueSample(
        "Notifications.macOS.ActionReceived.Alert", /*sample=*/true,
        /*expected_count=*/1);
    histogram_tester.ExpectUniqueSample(
        "Notifications.macOS.ActionReceived.Banner", /*sample=*/true,
        /*expected_count=*/1);
  }
}

TEST_P(UNNotificationPlatformBridgeMacPermissionStatusTest, PermissionStatus) {
  if (@available(macOS 10.14, *)) {
    UNNotificationPermissionStatus permission_status = GetParam();

    base::scoped_nsobject<FakeUNUserNotificationCenter> center(
        [[FakeUNUserNotificationCenter alloc] init]);
    [[center settings] setAlertStyle:UNAlertStyleBanner];

    switch (permission_status) {
      case UNNotificationPermissionStatus::kPermissionDenied:
        [[center settings] setAuthorizationStatus:UNAuthorizationStatusDenied];
        break;
      case UNNotificationPermissionStatus::kPermissionGranted:
        [[center settings]
            setAuthorizationStatus:UNAuthorizationStatusAuthorized];
        break;
      default:
        [[center settings]
            setAuthorizationStatus:UNAuthorizationStatusNotDetermined];
        break;
    }

    base::HistogramTester histogram_tester;
    auto bridge = std::make_unique<NotificationPlatformBridgeMacUNNotification>(
        static_cast<UNUserNotificationCenter*>(center.get()),
        alert_dispatcher_.get());

    histogram_tester.ExpectTotalCount(
        "Notifications.Permissions.UNNotification.Banners.PermissionStatus", 1);
    histogram_tester.ExpectBucketCount(
        "Notifications.Permissions.UNNotification.Banners.PermissionStatus",
        permission_status, 1);
  }
}

INSTANTIATE_TEST_SUITE_P(
    UNNotificationPlatformBridgeMacPermissionStatusTest,
    UNNotificationPlatformBridgeMacPermissionStatusTest,
    testing::Values(UNNotificationPermissionStatus::kNotRequestedYet,
                    UNNotificationPermissionStatus::kPermissionDenied,
                    UNNotificationPermissionStatus::kPermissionGranted));

TEST_P(UNNotificationPlatformBridgeMacBannerStyleTest, NotificationStyle) {
  if (@available(macOS 10.14, *)) {
    UNNotificationStyle notification_style = GetParam();

    base::scoped_nsobject<FakeUNUserNotificationCenter> center(
        [[FakeUNUserNotificationCenter alloc] init]);
    [[center settings] setAuthorizationStatus:UNAuthorizationStatusAuthorized];

    switch (notification_style) {
      case UNNotificationStyle::kBanners:
        [[center settings] setAlertStyle:UNAlertStyleBanner];
        break;
      case UNNotificationStyle::kAlerts:
        [[center settings] setAlertStyle:UNAlertStyleAlert];
        break;
      default:
        [[center settings] setAlertStyle:UNAlertStyleNone];
        break;
    }

    base::HistogramTester histogram_tester;
    auto bridge = std::make_unique<NotificationPlatformBridgeMacUNNotification>(
        static_cast<UNUserNotificationCenter*>(center.get()),
        alert_dispatcher_.get());

    histogram_tester.ExpectTotalCount(
        "Notifications.Permissions.UNNotification.Banners.Style", 1);
    histogram_tester.ExpectBucketCount(
        "Notifications.Permissions.UNNotification.Banners.Style",
        notification_style, 1);
  }
}

INSTANTIATE_TEST_SUITE_P(UNNotificationPlatformBridgeMacBannerStyleTest,
                         UNNotificationPlatformBridgeMacBannerStyleTest,
                         testing::Values(UNNotificationStyle::kNone,
                                         UNNotificationStyle::kBanners,
                                         UNNotificationStyle::kAlerts));

TEST_F(UNNotificationPlatformBridgeMacTest, NotificationResponse) {
  if (@available(macOS 10.14, *)) {
    base::HistogramTester histogram_tester;

    base::scoped_nsobject<FakeUNNotificationResponse> fakeResponse =
        CreateFakeUNNotificationResponse(@{
          notification_constants::kNotificationOrigin : @"https://google.com",
          notification_constants::kNotificationId : @"notificationId",
          notification_constants::kNotificationProfileId : @"profileId",
          notification_constants::kNotificationIncognito : @YES,
          notification_constants::kNotificationType : @0,
          notification_constants::
          kNotificationCreatorPid : @(base::GetCurrentProcId()),
        });

    [[center_ delegate]
                userNotificationCenter:static_cast<UNUserNotificationCenter*>(
                                           center_.get())
        didReceiveNotificationResponse:static_cast<UNNotificationResponse*>(
                                           fakeResponse.get())
                 withCompletionHandler:^{
                 }];

    // Handling responses is async, make sure we wait for all tasks to complete.
    task_environment_.RunUntilIdle();

    histogram_tester.ExpectUniqueSample(
        "Notifications.macOS.ActionReceived.Banner", /*sample=*/true,
        /*expected_count=*/1);
  }
}
