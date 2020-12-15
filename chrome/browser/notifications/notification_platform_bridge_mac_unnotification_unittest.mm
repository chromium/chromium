// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UserNotifications/UserNotifications.h>

#include "base/bind.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/notifications/notification_platform_bridge_mac_unnotification.h"
#include "chrome/browser/notifications/unnotification_metrics.h"
#include "chrome/browser/ui/cocoa/notifications/notification_constants_mac.h"
#include "chrome/browser/ui/cocoa/notifications/unnotification_builder_mac.h"
#include "chrome/browser/ui/cocoa/notifications/unnotification_response_builder_mac.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "ui/message_center/public/cpp/notification.h"
#include "url/gurl.h"

// TODO(crbug/1146412): Move the mock classes to a separate file to avoid name
// clashes.
API_AVAILABLE(macosx(10.14))
@interface FakeNotification : NSObject
@property(nonatomic, retain) UNNotificationRequest* request;
@end

@implementation FakeNotification
@synthesize request;
@end

API_AVAILABLE(macosx(10.14))
@interface FakeUNNotificationSettings : NSObject
@property(nonatomic, assign) UNAlertStyle alertStyle;
@property(nonatomic, assign) UNAuthorizationStatus authorizationStatus;
@end

@implementation FakeUNNotificationSettings
@synthesize alertStyle;
@synthesize authorizationStatus;
@end

API_AVAILABLE(macosx(10.14))
@interface FakeUNUserNotificationCenter : NSObject
@property(nonatomic, assign) base::scoped_nsobject<FakeUNNotificationSettings>
    settings;
- (instancetype)init;
// Need to provide a nop implementation of setDelegate as it is
// used during the setup of the bridge.
- (void)setDelegate:(id<UNUserNotificationCenterDelegate>)delegate;
- (void)removeAllDeliveredNotifications;
- (void)setNotificationCategories:(NSSet<UNNotificationCategory*>*)categories;
- (void)replaceContentForRequestWithIdentifier:(NSString*)requestIdentifier
                            replacementContent:
                                (UNMutableNotificationContent*)content
                             completionHandler:
                                 (void (^)(NSError* _Nullable error))
                                     notificationDelivered;
- (void)addNotificationRequest:(UNNotificationRequest*)request
         withCompletionHandler:(void (^)(NSError* error))completionHandler;
- (void)getDeliveredNotificationsWithCompletionHandler:
    (void (^)(NSArray<UNNotification*>* notifications))completionHandler;
- (void)getNotificationCategoriesWithCompletionHandler:
    (void (^)(NSSet<UNNotificationCategory*>* categories))completionHandler;
- (void)requestAuthorizationWithOptions:(UNAuthorizationOptions)options
                      completionHandler:(void (^)(BOOL granted, NSError* error))
                                            completionHandler;
- (void)removeDeliveredNotificationsWithIdentifiers:
    (NSArray<NSString*>*)identifiers;
- (void)getNotificationSettingsWithCompletionHandler:
    (void (^)(UNNotificationSettings* settings))completionHandler;
@end

@implementation FakeUNUserNotificationCenter {
  base::scoped_nsobject<NSMutableArray> _banners;
  base::scoped_nsobject<NSSet> _categories;
}

@synthesize settings;

- (instancetype)init {
  if ((self = [super init])) {
    settings.reset([[FakeUNNotificationSettings alloc] init]);
    _banners.reset([[NSMutableArray alloc] init]);
    _categories.reset([[NSSet alloc] init]);
  }
  return self;
}

- (void)setDelegate:(id<UNUserNotificationCenterDelegate>)delegate {
}

- (void)removeAllDeliveredNotifications {
  [_banners removeAllObjects];
}

- (void)setNotificationCategories:(NSSet<UNNotificationCategory*>*)categories {
  _categories.reset([categories copy]);
}

- (void)replaceContentForRequestWithIdentifier:(NSString*)requestIdentifier
                            replacementContent:
                                (UNMutableNotificationContent*)content
                             completionHandler:
                                 (void (^)(NSError* _Nullable error))
                                     notificationDelivered {
  UNNotificationRequest* request =
      [UNNotificationRequest requestWithIdentifier:requestIdentifier
                                           content:content
                                           trigger:nil];
  base::scoped_nsobject<FakeNotification> notification(
      [[FakeNotification alloc] init]);
  [notification setRequest:request];
  [_banners addObject:notification];
  notificationDelivered(/*error=*/nil);
}

- (void)addNotificationRequest:(UNNotificationRequest*)request
         withCompletionHandler:(void (^)(NSError* error))completionHandler {
  base::scoped_nsobject<FakeNotification> notification(
      [[FakeNotification alloc] init]);
  [notification setRequest:request];
  [_banners addObject:notification];
  completionHandler(/*error=*/nil);
}

- (void)getDeliveredNotificationsWithCompletionHandler:
    (void (^)(NSArray<UNNotification*>* notifications))completionHandler {
  completionHandler([[_banners copy] autorelease]);
}

- (void)getNotificationCategoriesWithCompletionHandler:
    (void (^)(NSSet<UNNotificationCategory*>* categories))completionHandler {
  completionHandler([[_categories copy] autorelease]);
}

- (void)requestAuthorizationWithOptions:(UNAuthorizationOptions)options
                      completionHandler:(void (^)(BOOL granted, NSError* error))
                                            completionHandler {
  completionHandler(/*granted=*/YES, /*error=*/nil);
}

- (void)removeDeliveredNotificationsWithIdentifiers:
    (NSArray<NSString*>*)identifiers {
  [_banners filterUsingPredicate:[NSPredicate predicateWithBlock:^BOOL(
                                                  UNNotification* notification,
                                                  NSDictionary* bindings) {
              NSString* toastId = [[[[notification request] content] userInfo]
                  objectForKey:notification_constants::kNotificationId];
              return ![identifiers containsObject:toastId];
            }]];
}

- (void)getNotificationSettingsWithCompletionHandler:
    (void (^)(UNNotificationSettings* settings))completionHandler {
  completionHandler(static_cast<UNNotificationSettings*>(settings.get()));
}

@end

using message_center::Notification;

class UNNotificationPlatformBridgeMacTest : public testing::Test {
 public:
  UNNotificationPlatformBridgeMacTest()
      : manager_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override {
    ASSERT_TRUE(manager_.SetUp());
    profile_ = manager_.CreateTestingProfile("Moe");
    if (@available(macOS 10.14, *)) {
      center_.reset([[FakeUNUserNotificationCenter alloc] init]);
      [[center_ settings] setAlertStyle:UNAlertStyleBanner];
      [[center_ settings]
          setAuthorizationStatus:UNAuthorizationStatusAuthorized];
      bridge_ = std::make_unique<NotificationPlatformBridgeMacUNNotification>(
          static_cast<UNUserNotificationCenter*>(center_.get()));
    }
  }

 protected:
  Notification CreateNotification(const std::string& notificationId = "id1") {
    GURL url("https://gmail.com");

    Notification notification(
        message_center::NOTIFICATION_TYPE_SIMPLE, notificationId,
        base::UTF8ToUTF16("Title"), base::UTF8ToUTF16("Context"), gfx::Image(),
        base::UTF8ToUTF16("Notifier's Name"), url,
        message_center::NotifierId(url), message_center::RichNotificationData(),
        base::MakeRefCounted<message_center::NotificationDelegate>());

    return notification;
  }

  API_AVAILABLE(macosx(10.14))
  base::scoped_nsobject<FakeUNUserNotificationCenter> center_;
  API_AVAILABLE(macosx(10.14))
  std::unique_ptr<NotificationPlatformBridgeMacUNNotification> bridge_;
  TestingProfile* profile_ = nullptr;
  base::HistogramTester histogram_tester_;

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager manager_;
};

class UNNotificationPlatformBridgeMacPermissionStatusTest
    : public UNNotificationPlatformBridgeMacTest,
      public testing::WithParamInterface<UNNotificationPermissionStatus> {};

class UNNotificationPlatformBridgeMacBannerStyleTest
    : public UNNotificationPlatformBridgeMacTest,
      public testing::WithParamInterface<UNNotificationStyle> {};

TEST_F(UNNotificationPlatformBridgeMacTest, TestDisplay) {
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
      EXPECT_NSEQ(@"Title", [delivered_content title]);
      EXPECT_NSEQ(@"Context", [delivered_content body]);
      EXPECT_NSEQ(@"gmail.com", [delivered_content subtitle]);
    }];

    [center_ getNotificationCategoriesWithCompletionHandler:^(
                 NSSet<UNNotificationCategory*>* categories) {
      EXPECT_EQ(1u, [categories count]);
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

    base::RunLoop run_loop;
    int notification_count = -1;
    bridge_->GetDisplayed(
        profile_,
        base::BindLambdaForTesting([&](std::set<std::string> notifications,
                                       bool supports_synchronization) {
          notification_count = notifications.size();
          run_loop.Quit();
        }));
    run_loop.Run();
    EXPECT_EQ(1, notification_count);
  }
}

TEST_F(UNNotificationPlatformBridgeMacTest,
       TestGetDisplayedMultipleNotifications) {
  if (@available(macOS 10.14, *)) {
    Notification first_notification = CreateNotification("id1");

    bridge_->Display(NotificationHandler::Type::WEB_PERSISTENT, profile_,
                     first_notification, nullptr);

    Notification second_notification = CreateNotification("id2");
    bridge_->Display(NotificationHandler::Type::WEB_PERSISTENT, profile_,
                     second_notification, nullptr);

    base::RunLoop run_loop;
    int notification_count = -1;
    bridge_->GetDisplayed(
        profile_,
        base::BindLambdaForTesting([&](std::set<std::string> notifications,
                                       bool supports_synchronization) {
          notification_count = notifications.size();
          run_loop.Quit();
        }));
    run_loop.Run();
    EXPECT_EQ(2, notification_count);

    [center_ getNotificationCategoriesWithCompletionHandler:^(
                 NSSet<UNNotificationCategory*>* categories) {
      EXPECT_EQ(2u, [categories count]);
    }];
  }
}

TEST_F(UNNotificationPlatformBridgeMacTest, TestQuitRemovesNotifications) {
  if (@available(macOS 10.14, *)) {
    Notification notification = CreateNotification();

    bridge_->Display(NotificationHandler::Type::WEB_PERSISTENT, profile_,
                     notification, nullptr);

    [center_ getDeliveredNotificationsWithCompletionHandler:^(
                 NSArray<UNNotification*>* _Nonnull notifications) {
      EXPECT_EQ(1u, [notifications count]);
    }];

    bridge_.reset();

    // The destructor of the bridge_ will call removeAllDeliveredNotifications.
    [center_ getDeliveredNotificationsWithCompletionHandler:^(
                 NSArray<UNNotification*>* _Nonnull notifications) {
      EXPECT_EQ(0u, [notifications count]);
    }];
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
        message_center::ButtonInfo(base::UTF8ToUTF16("Button 1")),
        message_center::ButtonInfo(base::UTF8ToUTF16("Button 2"))};
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

    [center_ getNotificationCategoriesWithCompletionHandler:^(
                 NSSet<UNNotificationCategory*>* categories) {
      ASSERT_EQ(1u, [categories count]);
      EXPECT_NSEQ(@"id1", [[categories anyObject] identifier]);
    }];
  }
}

TEST_F(UNNotificationPlatformBridgeMacTest, TestCloseRemovesCategory) {
  if (@available(macOS 10.14, *)) {
    Notification first_notification = CreateNotification();

    bridge_->Display(NotificationHandler::Type::WEB_PERSISTENT, profile_,
                     first_notification, nullptr);

    [center_ getNotificationCategoriesWithCompletionHandler:^(
                 NSSet<UNNotificationCategory*>* categories) {
      EXPECT_EQ(1u, [categories count]);
    }];

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

    [center_ getNotificationCategoriesWithCompletionHandler:^(
                 NSSet<UNNotificationCategory*>* categories) {
      ASSERT_EQ(1u, [categories count]);
      EXPECT_NSEQ(@"id2", [[categories anyObject] identifier]);
    }];
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
        static_cast<UNUserNotificationCenter*>(center.get()));

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
        static_cast<UNUserNotificationCenter*>(center.get()));

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
