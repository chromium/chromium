// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/notifications/alert_dispatcher_mojo.h"

#include <memory>

#include "base/callback.h"
#include "base/mac/scoped_nsobject.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/time/time.h"
#import "chrome/browser/notifications/mac_notification_provider_factory.h"
#include "chrome/services/mac_notifications/public/mojom/mac_notifications.mojom.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

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

std::vector<mac_notifications::mojom::NotificationIdentifierPtr>
CreateOneNotificationList() {
  std::vector<mac_notifications::mojom::NotificationIdentifierPtr> alerts;
  auto profile_id = mac_notifications::mojom::ProfileIdentifier::New(
      "profileId", /*incognito=*/true);
  alerts.push_back(mac_notifications::mojom::NotificationIdentifier::New(
      "notificationId", std::move(profile_id)));
  return alerts;
}

class FakeMacNotificationProviderFactory
    : public MacNotificationProviderFactory,
      public mac_notifications::mojom::MacNotificationProvider {
 public:
  explicit FakeMacNotificationProviderFactory(base::OnceClosure on_disconnect)
      : on_disconnect_(std::move(on_disconnect)) {}
  ~FakeMacNotificationProviderFactory() override = default;

  // MacNotificationProviderFactory:
  mojo::Remote<mac_notifications::mojom::MacNotificationProvider>
  LaunchProvider(bool in_process) override {
    EXPECT_FALSE(in_process);
    mojo::Remote<mac_notifications::mojom::MacNotificationProvider> remote;
    provider_receiver_.Bind(remote.BindNewPipeAndPassReceiver());
    provider_receiver_.set_disconnect_handler(std::move(on_disconnect_));
    return remote;
  }

  // mac_notifications::mojom::MacNotificationProvider:
  void BindNotificationService(
      mojo::PendingReceiver<mac_notifications::mojom::MacNotificationService>
          service,
      mojo::PendingRemote<
          mac_notifications::mojom::MacNotificationActionHandler> handler)
      override {
    service_receiver_.Bind(std::move(service));
    handler_remote_.Bind(std::move(handler));
  }

  MockNotificationService& service() { return mock_service_; }

  void disconnect() {
    handler_remote_.reset();
    service_receiver_.reset();
    provider_receiver_.reset();
  }

 private:
  base::OnceClosure on_disconnect_;
  mojo::Receiver<mac_notifications::mojom::MacNotificationProvider>
      provider_receiver_{this};
  MockNotificationService mock_service_;
  mojo::Receiver<mac_notifications::mojom::MacNotificationService>
      service_receiver_{&mock_service_};
  mojo::Remote<mac_notifications::mojom::MacNotificationActionHandler>
      handler_remote_;
};

}  // namespace

class AlertDispatcherMojoTest : public testing::Test {
 public:
  AlertDispatcherMojoTest() {
    auto provider_factory =
        std::make_unique<FakeMacNotificationProviderFactory>(
            on_disconnect_.Get());
    provider_factory_ = provider_factory.get();
    alert_dispatcher_.reset([[AlertDispatcherMojo alloc]
        initWithProviderFactory:std::move(provider_factory)]);
  }

  ~AlertDispatcherMojoTest() override {
    provider_factory_->disconnect();
    task_environment_.RunUntilIdle();
  }

  MockNotificationService& service() { return provider_factory_->service(); }

 protected:
  void ExpectDisconnect(base::OnceClosure callback) {
    EXPECT_CALL(on_disconnect_, Run())
        .WillOnce(base::test::RunOnceClosure(std::move(callback)));
  }

  void ExpectKeepConnected() {
    EXPECT_CALL(on_disconnect_, Run()).Times(0);
    // Run remaining tasks to see if we get disconnected.
    task_environment_.RunUntilIdle();
  }

  void EmulateNoNotifications() {
    EXPECT_CALL(service(), GetDisplayedNotifications)
        .WillOnce([](mac_notifications::mojom::ProfileIdentifierPtr profile,
                     MockNotificationService::GetDisplayedNotificationsCallback
                         callback) {
          // Emulate an empty list of alerts.
          std::move(callback).Run({});
        });
  }

  void EmulateOneNotification(base::OnceClosure callback) {
    EXPECT_CALL(service(), GetDisplayedNotifications)
        .WillOnce(testing::DoAll(
            base::test::RunOnceClosure(std::move(callback)),
            [](mac_notifications::mojom::ProfileIdentifierPtr profile,
               MockNotificationService::GetDisplayedNotificationsCallback
                   callback) {
              // Emulate one remaining notification.
              std::move(callback).Run(CreateOneNotificationList());
            }));
  }

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::MockOnceClosure on_disconnect_;
  base::scoped_nsobject<AlertDispatcherMojo> alert_dispatcher_;
  FakeMacNotificationProviderFactory* provider_factory_ = nullptr;
};

TEST_F(AlertDispatcherMojoTest, CloseAllNotifications) {
  base::RunLoop run_loop;
  // Expect that we disconnect after closing all notifications.
  ExpectDisconnect(run_loop.QuitClosure());
  EXPECT_CALL(service(), CloseAllNotifications);
  [alert_dispatcher_ closeAllNotifications];
  run_loop.Run();
}

TEST_F(AlertDispatcherMojoTest, CloseNotificationAndDisconnect) {
  base::RunLoop run_loop;
  // Expect that we disconnect after closing the last notification.
  ExpectDisconnect(run_loop.QuitClosure());
  EXPECT_CALL(service(), CloseNotification)
      .WillOnce(
          [](mac_notifications::mojom::NotificationIdentifierPtr identifier) {
            EXPECT_EQ("notificationId", identifier->id);
            EXPECT_EQ("profileId", identifier->profile->id);
            EXPECT_TRUE(identifier->profile->incognito);
          });
  EmulateNoNotifications();
  [alert_dispatcher_ closeNotificationWithId:@"notificationId"
                                   profileId:@"profileId"
                                   incognito:YES];
  run_loop.Run();
}

TEST_F(AlertDispatcherMojoTest, CloseNotificationAndKeepConnected) {
  base::RunLoop run_loop;
  // Expect that we continue running if there are remaining notifications.
  EXPECT_CALL(service(), CloseNotification);
  EmulateOneNotification(run_loop.QuitClosure());
  [alert_dispatcher_ closeNotificationWithId:@"notificationId"
                                   profileId:@"profileId"
                                   incognito:YES];
  run_loop.Run();
  ExpectKeepConnected();
}

TEST_F(AlertDispatcherMojoTest, CloseThenDispatchNotificationAndKeepConnected) {
  base::RunLoop run_loop;
  // Expect that we continue running when showing a new notification just after
  // closing the last one.
  EXPECT_CALL(service(), CloseNotification);
  EmulateNoNotifications();
  [alert_dispatcher_ closeNotificationWithId:@"notificationId"
                                   profileId:@"profileId"
                                   incognito:YES];

  EXPECT_CALL(service(), DisplayNotification)
      .WillOnce(base::test::RunOnceClosure(run_loop.QuitClosure()));
  [alert_dispatcher_ dispatchNotification:@{}];

  run_loop.Run();
  ExpectKeepConnected();
}

TEST_F(AlertDispatcherMojoTest, CloseProfileNotificationsAndDisconnect) {
  base::RunLoop run_loop;
  // Expect that we disconnect after closing the last notification.
  ExpectDisconnect(run_loop.QuitClosure());
  EXPECT_CALL(service(), CloseNotificationsForProfile)
      .WillOnce([](mac_notifications::mojom::ProfileIdentifierPtr profile) {
        EXPECT_EQ("profileId", profile->id);
        EXPECT_TRUE(profile->incognito);
      });
  EmulateNoNotifications();
  [alert_dispatcher_ closeNotificationsWithProfileId:@"profileId"
                                           incognito:YES];
  run_loop.Run();
}

TEST_F(AlertDispatcherMojoTest, CloseAndDisconnectTiming) {
  base::HistogramTester histograms;
  // Show a new notification.
  EXPECT_CALL(service(), DisplayNotification);
  [alert_dispatcher_ dispatchNotification:@{}];

  // Wait for 30 seconds and close the notification.
  auto delay = base::TimeDelta::FromSeconds(30);
  task_environment_.FastForwardBy(delay);

  // Expect that we disconnect after closing the last notification.
  base::RunLoop run_loop;
  ExpectDisconnect(run_loop.QuitClosure());
  EmulateNoNotifications();
  EXPECT_CALL(service(), CloseNotification);
  [alert_dispatcher_ closeNotificationWithId:@"notificationId"
                                   profileId:@"profileId"
                                   incognito:YES];

  // Verify that we log the runtime length and no unexpected kill.
  run_loop.Run();
  histograms.ExpectUniqueTimeSample("Notifications.macOS.ServiceProcessRuntime",
                                    delay, /*expected_count=*/1);
  histograms.ExpectTotalCount("Notifications.macOS.ServiceProcessKilled",
                              /*count=*/0);
}

TEST_F(AlertDispatcherMojoTest, KillServiceTiming) {
  base::HistogramTester histograms;
  // Show a new notification.
  EXPECT_CALL(service(), DisplayNotification);
  [alert_dispatcher_ dispatchNotification:@{}];

  // Wait for 30 seconds and terminate the service.
  auto delay = base::TimeDelta::FromSeconds(30);
  task_environment_.FastForwardBy(delay);
  // Simulate a dying service process.
  provider_factory_->disconnect();

  // Run remaining tasks as we can't observe the disconnect callback.
  task_environment_.RunUntilIdle();
  // Verify that we log the runtime length and an unexpected kill.
  histograms.ExpectUniqueTimeSample("Notifications.macOS.ServiceProcessRuntime",
                                    delay, /*expected_count=*/1);
  histograms.ExpectUniqueTimeSample("Notifications.macOS.ServiceProcessKilled",
                                    delay, /*expected_count=*/1);
}
