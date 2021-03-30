// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/mac/scoped_nsobject.h"
#include "base/process/process_handle.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#import "chrome/browser/notifications/notification_alert_service_bridge.h"
#include "chrome/services/mac_notifications/public/cpp/notification_constants_mac.h"
#include "chrome/services/mac_notifications/public/cpp/notification_operation.h"
#include "chrome/services/mac_notifications/public/mojom/mac_notifications.mojom.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"

namespace {

class MockNotificationService
    : public mac_notifications::mojom::MacNotificationService {
 public:
  MOCK_METHOD(void,
              DisplayNotification,
              (mac_notifications::mojom::NotificationPtr),
              (override));
  MOCK_METHOD(void,
              GetDisplayedNotifications,
              (mac_notifications::mojom::ProfileIdentifierPtr,
               GetDisplayedNotificationsCallback),
              (override));
  MOCK_METHOD(void,
              CloseNotification,
              (mac_notifications::mojom::NotificationIdentifierPtr),
              (override));
  MOCK_METHOD(void,
              CloseNotificationsForProfile,
              (mac_notifications::mojom::ProfileIdentifierPtr),
              (override));
  MOCK_METHOD(void, CloseAllNotifications, (), (override));
};

class MockNotificationProvider
    : public mac_notifications::mojom::MacNotificationProvider {
 public:
  MOCK_METHOD(
      void,
      BindNotificationService,
      (mojo::PendingReceiver<mac_notifications::mojom::MacNotificationService>,
       mojo::PendingRemote<
           mac_notifications::mojom::MacNotificationActionHandler>),
      (override));
};

}  // namespace

class NotificationAlertServiceBridgeTest : public testing::Test {
 public:
  NotificationAlertServiceBridgeTest() {
    base::RunLoop run_loop;
    EXPECT_CALL(mock_provider_, BindNotificationService)
        .WillOnce(
            [&](mojo::PendingReceiver<
                    mac_notifications::mojom::MacNotificationService>
                    service_receiver,
                mojo::PendingRemote<
                    mac_notifications::mojom::MacNotificationActionHandler>
                    handler_remote) {
              service_receiver_.Bind(std::move(service_receiver));
              handler_remote_.Bind(std::move(handler_remote));
              run_loop.Quit();
            });
    mojo::Remote<mac_notifications::mojom::MacNotificationProvider>
        provider_remote;
    provider_receiver_.Bind(provider_remote.BindNewPipeAndPassReceiver());
    bridge_.reset([[NotificationAlertServiceBridge alloc]
        initWithDisconnectHandler:on_disconnect_.Get()
                    actionHandler:on_action_.Get()
                         provider:std::move(provider_remote)]);
    run_loop.Run();
  }

  ~NotificationAlertServiceBridgeTest() override {
    // Make sure we run all remaining posted tasks that may use
    // |profile_manager_| before we destroy it.
    task_environment_.RunUntilIdle();
  }

  // testing::Test:
  void SetUp() override {
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  base::MockOnceClosure on_disconnect_;
  base::MockRepeatingClosure on_action_;
  MockNotificationService mock_service_;
  mojo::Receiver<mac_notifications::mojom::MacNotificationService>
      service_receiver_{&mock_service_};
  mojo::Remote<mac_notifications::mojom::MacNotificationActionHandler>
      handler_remote_;
  MockNotificationProvider mock_provider_;
  mojo::Receiver<mac_notifications::mojom::MacNotificationProvider>
      provider_receiver_{&mock_provider_};
  base::scoped_nsobject<NotificationAlertServiceBridge> bridge_;
};

TEST_F(NotificationAlertServiceBridgeTest, DisconnectHandler) {
  base::RunLoop run_loop;
  EXPECT_CALL(on_disconnect_, Run).WillOnce([&]() { run_loop.Quit(); });
  provider_receiver_.reset();
  run_loop.Run();
}

TEST_F(NotificationAlertServiceBridgeTest, DeliverNotification) {
  base::RunLoop run_loop;
  EXPECT_CALL(mock_service_, DisplayNotification)
      .WillOnce([&](mac_notifications::mojom::NotificationPtr notification) {
        const mac_notifications::mojom::NotificationIdentifierPtr& identifier =
            notification->meta->id;
        EXPECT_EQ("notificationId", identifier->id);
        EXPECT_EQ("profileId", identifier->profile->id);
        EXPECT_TRUE(identifier->profile->incognito);

        EXPECT_EQ(u"title", notification->title);
        EXPECT_EQ(u"subtitle", notification->subtitle);
        EXPECT_EQ(u"body", notification->body);
        EXPECT_FALSE(notification->renotify);
        EXPECT_TRUE(notification->show_settings_button);

        ASSERT_EQ(2u, notification->buttons.size());
        EXPECT_EQ(u"button1", notification->buttons[0]->title);
        EXPECT_EQ(u"button2", notification->buttons[1]->title);
        run_loop.Quit();
      });

  [bridge_ deliverNotification:@{
    notification_constants::kNotificationId : @"notificationId",
    notification_constants::kNotificationProfileId : @"profileId",
    notification_constants::kNotificationIncognito : @YES,
    notification_constants::kNotificationTitle : @"title",
    notification_constants::kNotificationSubTitle : @"subtitle",
    notification_constants::kNotificationInformativeText : @"body",
    notification_constants::kNotificationRenotify : @NO,
    notification_constants::kNotificationHasSettingsButton : @YES,
    notification_constants::kNotificationButtonOne : @"button1",
    notification_constants::kNotificationButtonTwo : @"button2",
  }];
  run_loop.Run();
}

TEST_F(NotificationAlertServiceBridgeTest, GetDisplayedAlertsForProfile) {
  EXPECT_CALL(mock_service_, GetDisplayedNotifications)
      .WillOnce([&](mac_notifications::mojom::ProfileIdentifierPtr profile,
                    MockNotificationService::GetDisplayedNotificationsCallback
                        callback) {
        ASSERT_TRUE(profile);
        EXPECT_EQ("profileId", profile->id);
        EXPECT_TRUE(profile->incognito);
        std::vector<mac_notifications::mojom::NotificationIdentifierPtr> alerts;
        alerts.push_back(mac_notifications::mojom::NotificationIdentifier::New(
            "notificationId", std::move(profile)));
        std::move(callback).Run(std::move(alerts));
      });

  base::RunLoop run_loop;
  base::RepeatingClosure run_loop_closure = run_loop.QuitClosure();
  [bridge_ getDisplayedAlertsForProfileId:@"profileId"
                                incognito:YES
                                    reply:^(NSArray* alerts) {
                                      ASSERT_EQ(1ul, [alerts count]);
                                      EXPECT_NSEQ(@"notificationId", alerts[0]);
                                      run_loop_closure.Run();
                                    }];
  run_loop.Run();
}

TEST_F(NotificationAlertServiceBridgeTest, GetAllDisplayedAlerts) {
  EXPECT_CALL(mock_service_, GetDisplayedNotifications)
      .WillOnce([&](mac_notifications::mojom::ProfileIdentifierPtr profile,
                    MockNotificationService::GetDisplayedNotificationsCallback
                        callback) {
        ASSERT_FALSE(profile);
        std::vector<mac_notifications::mojom::NotificationIdentifierPtr> alerts;
        alerts.push_back(mac_notifications::mojom::NotificationIdentifier::New(
            "notificationId", mac_notifications::mojom::ProfileIdentifier::New(
                                  "profileId", /*incognito=*/true)));
        std::move(callback).Run(std::move(alerts));
      });

  NSDictionary* expected = @{
    notification_constants::kNotificationId : @"notificationId",
    notification_constants::kNotificationProfileId : @"profileId",
    notification_constants::kNotificationIncognito : @YES,
  };

  base::RunLoop run_loop;
  base::RepeatingClosure run_loop_closure = run_loop.QuitClosure();
  [bridge_ getAllDisplayedAlertsWithReply:^(NSArray* alerts) {
    ASSERT_EQ(1ul, [alerts count]);
    EXPECT_NSEQ(expected, alerts[0]);
    run_loop_closure.Run();
  }];
  run_loop.Run();
}

TEST_F(NotificationAlertServiceBridgeTest, CloseNotification) {
  base::RunLoop run_loop;
  EXPECT_CALL(mock_service_, CloseNotification)
      .WillOnce(
          [&](mac_notifications::mojom::NotificationIdentifierPtr identifier) {
            ASSERT_TRUE(identifier);
            EXPECT_EQ("notificationId", identifier->id);
            ASSERT_TRUE(identifier->profile);
            EXPECT_EQ("profileId", identifier->profile->id);
            EXPECT_TRUE(identifier->profile->incognito);
            run_loop.Quit();
          });
  [bridge_ closeNotificationWithId:@"notificationId"
                         profileId:@"profileId"
                         incognito:YES];
  run_loop.Run();
}

TEST_F(NotificationAlertServiceBridgeTest, CloseProfileNotifications) {
  base::RunLoop run_loop;
  EXPECT_CALL(mock_service_, CloseNotificationsForProfile)
      .WillOnce([&](mac_notifications::mojom::ProfileIdentifierPtr profile) {
        ASSERT_TRUE(profile);
        ASSERT_EQ("profileId", profile->id);
        EXPECT_TRUE(profile->incognito);
        run_loop.Quit();
      });
  [bridge_ closeNotificationsWithProfileId:@"profileId" incognito:YES];
  run_loop.Run();
}

TEST_F(NotificationAlertServiceBridgeTest, CloseAllNotifications) {
  base::RunLoop run_loop;
  EXPECT_CALL(mock_service_, CloseAllNotifications).WillOnce([&]() {
    run_loop.Quit();
  });
  [bridge_ closeAllNotifications];
  run_loop.Run();
}

TEST_F(NotificationAlertServiceBridgeTest, OnNotificationAction) {
  base::HistogramTester histogram_tester;
  auto profile_identifier = mac_notifications::mojom::ProfileIdentifier::New(
      "profileId", /*incognito=*/true);
  auto notification_identifier =
      mac_notifications::mojom::NotificationIdentifier::New(
          "notificationId", std::move(profile_identifier));
  auto meta = mac_notifications::mojom::NotificationMetadata::New(
      std::move(notification_identifier), /*type=*/0, /*origin_url=*/GURL(),
      base::GetCurrentProcId());

  base::RunLoop run_loop;
  EXPECT_CALL(on_action_, Run).WillOnce([&]() { run_loop.Quit(); });

  auto action_info = mac_notifications::mojom::NotificationActionInfo::New(
      std::move(meta), NotificationOperation::NOTIFICATION_CLICK,
      /*button_index=*/-1, /*reply=*/base::nullopt);
  handler_remote_->OnNotificationAction(std::move(action_info));

  // TODO(knollr): verify expected notification action data.
  // Wait until the action has been handled.
  run_loop.Run();

  histogram_tester.ExpectUniqueSample(
      "Notifications.macOS.ActionReceived.Alert", /*sample=*/true,
      /*expected_count=*/1);
}
