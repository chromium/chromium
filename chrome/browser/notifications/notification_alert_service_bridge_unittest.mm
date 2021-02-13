// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>
#include <vector>

#include "base/mac/scoped_nsobject.h"
#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#import "chrome/browser/notifications/notification_alert_service_bridge.h"
#include "chrome/services/mac_notifications/public/cpp/notification_constants_mac.h"
#include "chrome/services/mac_notifications/public/mojom/mac_notifications.mojom.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"

namespace {

class MockNotificationService
    : public notifications::mojom::MacNotificationService {
 public:
  MOCK_METHOD(void,
              GetDisplayedNotifications,
              (notifications::mojom::ProfileIdentifierPtr,
               GetDisplayedNotificationsCallback),
              (override));
  MOCK_METHOD(void,
              CloseNotification,
              (notifications::mojom::NotificationIdentifierPtr),
              (override));
  MOCK_METHOD(void, CloseAllNotifications, (), (override));
};

class MockNotificationProvider
    : public notifications::mojom::MacNotificationProvider {
 public:
  MOCK_METHOD(
      void,
      BindNotificationService,
      (mojo::PendingReceiver<notifications::mojom::MacNotificationService>,
       mojo::PendingRemote<notifications::mojom::MacNotificationActionHandler>),
      (override));
};

}  // namespace

class NotificationAlertServiceBridgeTest : public testing::Test {
 public:
  NotificationAlertServiceBridgeTest() {
    base::RunLoop run_loop;
    EXPECT_CALL(mock_provider_, BindNotificationService)
        .WillOnce([&](mojo::PendingReceiver<
                          notifications::mojom::MacNotificationService>
                          service_receiver,
                      mojo::PendingRemote<
                          notifications::mojom::MacNotificationActionHandler>
                          handler_remote) {
          service_receiver_.Bind(std::move(service_receiver));
          handler_remote_.Bind(std::move(handler_remote));
          run_loop.Quit();
        });
    bridge_.reset([[NotificationAlertServiceBridge alloc]
        initWithDisconnectHandler:on_disconnect_.Get()
                         provider:provider_receiver_
                                      .BindNewPipeAndPassRemote()]);
    run_loop.Run();
  }

  ~NotificationAlertServiceBridgeTest() override = default;

 protected:
  content::BrowserTaskEnvironment task_environment_;
  base::MockOnceClosure on_disconnect_;
  MockNotificationService mock_service_;
  mojo::Receiver<notifications::mojom::MacNotificationService>
      service_receiver_{&mock_service_};
  mojo::Remote<notifications::mojom::MacNotificationActionHandler>
      handler_remote_;
  MockNotificationProvider mock_provider_;
  mojo::Receiver<notifications::mojom::MacNotificationProvider>
      provider_receiver_{&mock_provider_};
  base::scoped_nsobject<NotificationAlertServiceBridge> bridge_;
};

TEST_F(NotificationAlertServiceBridgeTest, DisconnectHandler) {
  base::RunLoop run_loop;
  EXPECT_CALL(on_disconnect_, Run).WillOnce([&]() { run_loop.Quit(); });
  provider_receiver_.reset();
  run_loop.Run();
}

TEST_F(NotificationAlertServiceBridgeTest, GetDisplayedAlertsForProfile) {
  EXPECT_CALL(mock_service_, GetDisplayedNotifications)
      .WillOnce([&](notifications::mojom::ProfileIdentifierPtr profile,
                    MockNotificationService::GetDisplayedNotificationsCallback
                        callback) {
        ASSERT_TRUE(profile);
        EXPECT_EQ("profileId", profile->id);
        EXPECT_TRUE(profile->incognito);
        std::vector<notifications::mojom::NotificationIdentifierPtr> alerts;
        alerts.push_back(notifications::mojom::NotificationIdentifier::New(
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
      .WillOnce([&](notifications::mojom::ProfileIdentifierPtr profile,
                    MockNotificationService::GetDisplayedNotificationsCallback
                        callback) {
        ASSERT_FALSE(profile);
        std::vector<notifications::mojom::NotificationIdentifierPtr> alerts;
        alerts.push_back(notifications::mojom::NotificationIdentifier::New(
            "notificationId", notifications::mojom::ProfileIdentifier::New(
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
          [&](notifications::mojom::NotificationIdentifierPtr identifier) {
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

TEST_F(NotificationAlertServiceBridgeTest, CloseAllNotifications) {
  base::RunLoop run_loop;
  EXPECT_CALL(mock_service_, CloseAllNotifications).WillOnce([&]() {
    run_loop.Quit();
  });
  [bridge_ closeAllNotifications];
  run_loop.Run();
}

TEST_F(NotificationAlertServiceBridgeTest, OnNotificationAction) {
  // TODO(knollr): pass and verify expected notification action data.
  handler_remote_->OnNotificationAction(
      notifications::mojom::NotificationActionInfo::New());
  // Wait until the action has been handled.
  task_environment_.RunUntilIdle();
}
