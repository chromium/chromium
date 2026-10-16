// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/mac/notification_dispatcher_mojo.h"

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/process/process_handle.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/notifications/mac/mac_notification_provider_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/notifications/notification_operation.h"
#include "chrome/services/mac_notifications/public/mojom/mac_notifications.mojom.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kNotificationId[] = "notification-id";
const char kProfileId[] = "profile-id";

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
               const std::optional<GURL>& origin,
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
  MOCK_METHOD(void,
              OkayToTerminateService,
              (OkayToTerminateServiceCallback),
              (override));
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

class FakeMacNotificationProviderFactory
    : public MacNotificationProviderFactory,
      public mac_notifications::mojom::MacNotificationProvider {
 public:
  explicit FakeMacNotificationProviderFactory(
      base::RepeatingClosure on_disconnect,
      mac_notifications::NotificationStyle notification_style =
          mac_notifications::NotificationStyle::kAlert)
      : MacNotificationProviderFactory(
            notification_style,
            notification_style == mac_notifications::NotificationStyle::kAppShim
                ? "app-id"
                : ""),
        on_disconnect_(std::move(on_disconnect)) {}
  ~FakeMacNotificationProviderFactory() override = default;

  // MacNotificationProviderFactory:
  mojo::Remote<mac_notifications::mojom::MacNotificationProvider>
  LaunchProvider() override {
    mojo::Remote<mac_notifications::mojom::MacNotificationProvider> remote;
    provider_receiver_.Bind(remote.BindNewPipeAndPassReceiver());
    provider_receiver_.set_disconnect_handler(
        base::BindOnce(&FakeMacNotificationProviderFactory::Disconnect,
                       base::Unretained(this)));
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

  mac_notifications::mojom::MacNotificationActionHandler* handler() {
    return handler_remote_.get();
  }

  void Disconnect() {
    handler_remote_.reset();
    service_receiver_.reset();
    provider_receiver_.reset();
    on_disconnect_.Run();
  }

  void DisconnectServiceOnly() {
    handler_remote_.reset();
    service_receiver_.reset();
  }

  bool is_service_connected() { return service_receiver_.is_bound(); }

 private:
  base::RepeatingClosure on_disconnect_;
  mojo::Receiver<mac_notifications::mojom::MacNotificationProvider>
      provider_receiver_{this};
  MockNotificationService mock_service_;
  mojo::Receiver<mac_notifications::mojom::MacNotificationService>
      service_receiver_{&mock_service_};
  mojo::Remote<mac_notifications::mojom::MacNotificationActionHandler>
      handler_remote_;
};

message_center::Notification CreateNotification() {
  return message_center::Notification(
      message_center::NOTIFICATION_TYPE_SIMPLE, kNotificationId, u"title",
      u"message", /*icon=*/ui::ImageModel(),
      /*display_source=*/std::u16string(), /*origin_url=*/GURL(),
      message_center::NotifierId(), message_center::RichNotificationData(),
      base::MakeRefCounted<message_center::NotificationDelegate>());
}

mac_notifications::mojom::NotificationMetadataPtr CreateNotificationMetadata() {
  auto profile_identifier = mac_notifications::mojom::ProfileIdentifier::New(
      kProfileId, /*incognito=*/true);
  auto notification_identifier =
      mac_notifications::mojom::NotificationIdentifier::New(
          kNotificationId, std::move(profile_identifier));
  base::FilePath user_data_dir;
  EXPECT_TRUE(base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir));
  return mac_notifications::mojom::NotificationMetadata::New(
      std::move(notification_identifier), /*notification_type=*/0,
      /*origin_url=*/GURL("https://example.com"), user_data_dir.value());
}

mac_notifications::mojom::NotificationActionInfoPtr
CreateNotificationActionInfo() {
  auto meta = CreateNotificationMetadata();
  return mac_notifications::mojom::NotificationActionInfo::New(
      std::move(meta), NotificationOperation::kClick,
      /*button_index=*/-1, /*reply=*/std::nullopt);
}

}  // namespace

class NotificationDispatcherMojoTest : public testing::Test {
 public:
  NotificationDispatcherMojoTest() = default;

  ~NotificationDispatcherMojoTest() override {
    base::RunLoop run_loop;
    ExpectDisconnect(run_loop.QuitClosure());
    provider_factory_->Disconnect();
    run_loop.Run();
  }

  // testing::Test:
  void SetUp() override {
    ASSERT_TRUE(testing_profile_manager_.SetUp());
    profile_ = testing_profile_manager_.CreateTestingProfile("profile");

    auto provider_factory =
        std::make_unique<FakeMacNotificationProviderFactory>(
            on_disconnect_.Get());
    provider_factory_ = provider_factory.get();

    // NotificationDispatcherMojo will query if it can terminate the service
    // at startup. Once that finishes it should disconnect due to inactivity.
    base::RunLoop run_loop;
    EmulateOkayToTerminate(/*can_terminate=*/true);
    ExpectDisconnect(run_loop.QuitClosure());

    notification_dispatcher_ = std::make_unique<NotificationDispatcherMojo>(
        std::move(provider_factory));
    run_loop.Run();
  }

  MockNotificationService& service() { return provider_factory_->service(); }

  mac_notifications::mojom::MacNotificationActionHandler* handler() {
    return provider_factory_->handler();
  }

 protected:
  void ExpectDisconnect(base::OnceClosure callback) {
    EXPECT_CALL(on_disconnect_, Run())
        .WillOnce(base::test::RunOnceClosure(std::move(callback)))
        .WillRepeatedly(testing::DoDefault());
  }

  void ExpectKeepConnected() {
    EXPECT_CALL(on_disconnect_, Run()).Times(0);
    // Run remaining tasks to see if we get disconnected.
    task_environment_.RunUntilIdle();
  }

  void EmulateOkayToTerminate(bool can_terminate,
                              base::OnceClosure callback = base::DoNothing()) {
    EXPECT_CALL(service(), OkayToTerminateService)
        .WillRepeatedly(testing::DoAll(
            base::test::RunOnceClosure(std::move(callback)),
            [can_terminate](
                MockNotificationService::OkayToTerminateServiceCallback
                    callback) { std::move(callback).Run(can_terminate); }));
  }

  void DisplayNotificationSync() {
    base::RunLoop run_loop;
    EXPECT_CALL(service(), DisplayNotification)
        .WillOnce(base::test::RunOnceClosure(run_loop.QuitClosure()));
    notification_dispatcher_->DisplayNotification(
        NotificationHandler::Type::WEB_PERSISTENT, profile_,
        CreateNotification());
    run_loop.Run();
  }

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingProfileManager testing_profile_manager_{
      TestingBrowserProcess::GetGlobal()};
  raw_ptr<Profile> profile_;
  base::MockRepeatingClosure on_disconnect_;
  std::unique_ptr<NotificationDispatcherMojo> notification_dispatcher_;
  raw_ptr<FakeMacNotificationProviderFactory> provider_factory_ = nullptr;
};

TEST_F(NotificationDispatcherMojoTest, CloseNotificationAndDisconnect) {
  DisplayNotificationSync();

  base::RunLoop run_loop;
  // Expect that we disconnect after closing the last notification.
  ExpectDisconnect(run_loop.QuitClosure());
  EXPECT_CALL(service(), CloseNotification)
      .WillOnce(
          [](mac_notifications::mojom::NotificationIdentifierPtr identifier) {
            EXPECT_EQ(kNotificationId, identifier->id);
            EXPECT_EQ(kProfileId, identifier->profile->id);
            EXPECT_TRUE(identifier->profile->incognito);
          });
  EmulateOkayToTerminate(/*can_terminate=*/true);
  notification_dispatcher_->CloseNotificationWithId(
      {kNotificationId, kProfileId, /*incognito=*/true});
  run_loop.Run();
}

TEST_F(NotificationDispatcherMojoTest, CloseNotificationAndKeepConnected) {
  DisplayNotificationSync();

  base::RunLoop run_loop;
  // Expect that we continue running if there are remaining notifications.
  EXPECT_CALL(service(), CloseNotification);
  EmulateOkayToTerminate(/*can_terminate=*/false, run_loop.QuitClosure());
  notification_dispatcher_->CloseNotificationWithId(
      {kNotificationId, kProfileId, /*incognito=*/true});
  run_loop.Run();
  ExpectKeepConnected();
}

TEST_F(NotificationDispatcherMojoTest,
       CloseThenDispatchNotificationAndKeepConnected) {
  base::RunLoop run_loop;
  DisplayNotificationSync();

  // Expect that we continue running when showing a new notification just after
  // closing the last one.
  EXPECT_CALL(service(), CloseNotification);
  EmulateOkayToTerminate(/*can_terminate=*/true);
  notification_dispatcher_->CloseNotificationWithId(
      {kNotificationId, kProfileId, /*incognito=*/true});

  EXPECT_CALL(service(), DisplayNotification)
      .WillOnce(base::test::RunOnceClosure(run_loop.QuitClosure()));
  notification_dispatcher_->DisplayNotification(
      NotificationHandler::Type::WEB_PERSISTENT, profile_,
      CreateNotification());

  run_loop.Run();
  ExpectKeepConnected();

  base::RunLoop run_loop2;
  // Expect that we disconnect after closing all notifications.
  ExpectDisconnect(run_loop2.QuitClosure());
  EXPECT_CALL(service(), CloseAllNotifications);
  notification_dispatcher_->CloseAllNotifications();
  run_loop2.Run();
}

TEST_F(NotificationDispatcherMojoTest, CloseProfileNotificationsAndDisconnect) {
  DisplayNotificationSync();

  base::RunLoop run_loop;
  // Expect that we disconnect after closing the last notification.
  ExpectDisconnect(run_loop.QuitClosure());
  EXPECT_CALL(service(), CloseNotificationsForProfile)
      .WillOnce([](mac_notifications::mojom::ProfileIdentifierPtr profile) {
        EXPECT_EQ(kProfileId, profile->id);
        EXPECT_TRUE(profile->incognito);
      });
  EmulateOkayToTerminate(/*can_terminate=*/true);
  notification_dispatcher_->CloseNotificationsWithProfileId(kProfileId,
                                                            /*incognito=*/true);
  run_loop.Run();
}

TEST_F(NotificationDispatcherMojoTest, CloseAndDisconnectTiming) {
  base::HistogramTester histograms;
  // Show a new notification.
  EXPECT_CALL(service(), DisplayNotification);
  notification_dispatcher_->DisplayNotification(
      NotificationHandler::Type::WEB_PERSISTENT, profile_,
      CreateNotification());

  // Wait for 30 seconds and close the notification.
  auto delay = base::Seconds(30);
  task_environment_.FastForwardBy(delay);

  // Expect that we disconnect after closing the last notification.
  base::RunLoop run_loop;
  ExpectDisconnect(run_loop.QuitClosure());
  EmulateOkayToTerminate(/*can_terminate=*/true);
  EXPECT_CALL(service(), CloseNotification);
  notification_dispatcher_->CloseNotificationWithId(
      {kNotificationId, kProfileId, /*incognito=*/true});

  // Verify that we log the runtime length, time to first response, and no
  // unexpected kill.
  run_loop.Run();
  histograms.ExpectUniqueTimeSample("Notifications.macOS.ServiceProcessRuntime",
                                    delay, /*expected_count=*/1);
  histograms.ExpectUniqueTimeSample(
      "Notifications.macOS.ServiceProcess.TimeToFirstResponse", delay,
      /*expected_count=*/1);
  histograms.ExpectTotalCount("Notifications.macOS.ServiceProcessKilled",
                              /*count=*/0);
}

TEST_F(NotificationDispatcherMojoTest, KillServiceTiming) {
  base::HistogramTester histograms;
  // Show a new notification.
  EXPECT_CALL(service(), DisplayNotification);
  notification_dispatcher_->DisplayNotification(
      NotificationHandler::Type::WEB_PERSISTENT, profile_,
      CreateNotification());

  // Wait for 30 seconds and terminate the service.
  auto delay = base::Seconds(30);
  task_environment_.FastForwardBy(delay);
  // Simulate a dying service process.
  provider_factory_->Disconnect();

  // Run remaining tasks as we can't observe the disconnect callback.
  task_environment_.RunUntilIdle();
  // Verify that we log the runtime length and an unexpected kill, and no
  // first-response time since the service never responded.
  histograms.ExpectUniqueTimeSample("Notifications.macOS.ServiceProcessRuntime",
                                    delay, /*expected_count=*/1);
  histograms.ExpectUniqueTimeSample("Notifications.macOS.ServiceProcessKilled",
                                    delay, /*expected_count=*/1);
  histograms.ExpectTotalCount(
      "Notifications.macOS.ServiceProcess.TimeToFirstResponse",
      /*count=*/0);
}

TEST_F(NotificationDispatcherMojoTest, DidActivateNotification) {
  base::HistogramTester histograms;
  // Show a new notification.
  EmulateOkayToTerminate(/*can_terminate=*/true);
  EXPECT_CALL(service(), DisplayNotification);
  notification_dispatcher_->DisplayNotification(
      NotificationHandler::Type::WEB_PERSISTENT, profile_,
      CreateNotification());

  // Wait until the action handler has been bound.
  task_environment_.RunUntilIdle();
  handler()->OnNotificationAction(CreateNotificationActionInfo());

  // Handling responses is async, make sure we wait for all tasks to complete.
  task_environment_.RunUntilIdle();
}

TEST_F(NotificationDispatcherMojoTest, TestUnexpectedDisconnectReconnects) {
  // Display a notification and verify that the service is running.
  DisplayNotificationSync();
  EXPECT_TRUE(provider_factory_->is_service_connected());

  // Disconnect after 30 seconds while there is still a notification on screen.
  task_environment_.FastForwardBy(base::Seconds(30));
  provider_factory_->Disconnect();
  EXPECT_FALSE(provider_factory_->is_service_connected());

  // Expect the service to be restarted after a short timeout.
  EmulateOkayToTerminate(/*can_terminate=*/false);
  task_environment_.FastForwardBy(base::Milliseconds(500));
  EXPECT_TRUE(provider_factory_->is_service_connected());
}

TEST_F(NotificationDispatcherMojoTest, TestReconnectBackoff) {
  // Display a notification and verify that the service is running.
  DisplayNotificationSync();
  // Disconnect after 30 seconds while there is still a notification on screen.
  task_environment_.FastForwardBy(base::Seconds(30));
  provider_factory_->Disconnect();

  // Verify the service hasn't restarted if not enough time has passed.
  task_environment_.FastForwardBy(base::Milliseconds(499));
  EXPECT_FALSE(provider_factory_->is_service_connected());
  // Expect the service to be restarted after a short timeout.
  EmulateOkayToTerminate(/*can_terminate=*/false);
  task_environment_.FastForwardBy(base::Milliseconds(1));
  EXPECT_TRUE(provider_factory_->is_service_connected());

  // Disconnect again immediately which should double the restart timeout.
  provider_factory_->Disconnect();

  // Verify the service hasn't restarted if not enough time has passed.
  task_environment_.FastForwardBy(base::Milliseconds(999));
  EXPECT_FALSE(provider_factory_->is_service_connected());
  // Expect the service to be restarted after a short timeout.
  EmulateOkayToTerminate(/*can_terminate=*/false);
  task_environment_.FastForwardBy(base::Milliseconds(1));
  EXPECT_TRUE(provider_factory_->is_service_connected());

  // After the service has responded and remained connected for > 10 seconds,
  // a subsequent disconnect should reset the restart timeout back to 500ms.
  task_environment_.FastForwardBy(base::Seconds(30));
  provider_factory_->Disconnect();
  task_environment_.FastForwardBy(base::Milliseconds(499));
  EXPECT_FALSE(provider_factory_->is_service_connected());
  EmulateOkayToTerminate(/*can_terminate=*/false);
  task_environment_.FastForwardBy(base::Milliseconds(1));
  EXPECT_TRUE(provider_factory_->is_service_connected());
}

TEST_F(NotificationDispatcherMojoTest,
       ServiceDisconnectRecoversWhenProviderStaysConnected) {
  DisplayNotificationSync();
  EXPECT_TRUE(provider_factory_->is_service_connected());

  // Simulate the service pipe closing while the provider stays connected (e.g.
  // a dummy provider or a shim that drops the service receiver).
  task_environment_.FastForwardBy(base::Seconds(30));
  base::test::TestFuture<void> disconnect_future;
  ExpectDisconnect(disconnect_future.GetCallback());
  provider_factory_->DisconnectServiceOnly();
  EXPECT_TRUE(disconnect_future.Wait());
  EXPECT_FALSE(provider_factory_->is_service_connected());

  // Expect the service to be restarted after the restart timer fires.
  EmulateOkayToTerminate(/*can_terminate=*/false);
  task_environment_.FastForwardBy(base::Milliseconds(500));
  EXPECT_TRUE(provider_factory_->is_service_connected());
}

TEST_F(NotificationDispatcherMojoTest,
       GetDisplayedNotificationsCallbacksRunOnServiceDisconnect) {
  DisplayNotificationSync();
  EXPECT_TRUE(provider_factory_->is_service_connected());

  base::test::TestFuture<std::set<std::string>, bool> profile_future;
  base::test::TestFuture<std::set<std::string>, bool> origin_future;
  base::test::TestFuture<base::flat_set<MacNotificationIdentifier>> all_future;

  notification_dispatcher_->GetDisplayedNotificationsForProfileId(
      kProfileId, /*incognito=*/false, profile_future.GetCallback());
  notification_dispatcher_->GetDisplayedNotificationsForProfileIdAndOrigin(
      kProfileId, /*incognito=*/false, GURL("https://example.com"),
      origin_future.GetCallback());
  notification_dispatcher_->GetAllDisplayedNotifications(
      all_future.GetCallback());

  // Disconnect the service pipe before it replies to the in-flight requests.
  provider_factory_->Disconnect();

  EXPECT_THAT(profile_future.Get<0>(), testing::IsEmpty());
  EXPECT_FALSE(profile_future.Get<1>());
  EXPECT_THAT(origin_future.Get<0>(), testing::IsEmpty());
  EXPECT_FALSE(origin_future.Get<1>());
  EXPECT_THAT(all_future.Get(), testing::IsEmpty());
}

TEST_F(NotificationDispatcherMojoTest,
       GetDisplayedNotificationsCallbacksRunOnServiceOnlyDisconnect) {
  DisplayNotificationSync();
  EXPECT_TRUE(provider_factory_->is_service_connected());

  base::test::TestFuture<std::set<std::string>, bool> profile_future;
  base::test::TestFuture<std::set<std::string>, bool> origin_future;
  base::test::TestFuture<base::flat_set<MacNotificationIdentifier>> all_future;

  notification_dispatcher_->GetDisplayedNotificationsForProfileId(
      kProfileId, /*incognito=*/false, profile_future.GetCallback());
  notification_dispatcher_->GetDisplayedNotificationsForProfileIdAndOrigin(
      kProfileId, /*incognito=*/false, GURL("https://example.com"),
      origin_future.GetCallback());
  notification_dispatcher_->GetAllDisplayedNotifications(
      all_future.GetCallback());

  // Disconnect only the service receiver while keeping the provider alive.
  provider_factory_->DisconnectServiceOnly();

  EXPECT_THAT(profile_future.Get<0>(), testing::IsEmpty());
  EXPECT_FALSE(profile_future.Get<1>());
  EXPECT_THAT(origin_future.Get<0>(), testing::IsEmpty());
  EXPECT_FALSE(origin_future.Get<1>());
  EXPECT_THAT(all_future.Get(), testing::IsEmpty());

  profile_future.Clear();
  origin_future.Clear();
  all_future.Clear();

  // Subsequent GetDisplayed* calls while service_ is disconnected (or before a
  // restart completes) should also invoke their callbacks rather than hanging.
  EXPECT_CALL(service(), GetDisplayedNotifications).WillOnce([&]() {
    provider_factory_->DisconnectServiceOnly();
  });

  notification_dispatcher_->GetDisplayedNotificationsForProfileId(
      kProfileId, /*incognito=*/false, profile_future.GetCallback());
  notification_dispatcher_->GetDisplayedNotificationsForProfileIdAndOrigin(
      kProfileId, /*incognito=*/false, GURL("https://example.com"),
      origin_future.GetCallback());
  notification_dispatcher_->GetAllDisplayedNotifications(
      all_future.GetCallback());

  EXPECT_THAT(profile_future.Get<0>(), testing::IsEmpty());
  EXPECT_FALSE(profile_future.Get<1>());
  EXPECT_THAT(origin_future.Get<0>(), testing::IsEmpty());
  EXPECT_FALSE(origin_future.Get<1>());
  EXPECT_THAT(all_future.Get(), testing::IsEmpty());
}

TEST_F(NotificationDispatcherMojoTest,
       NeverRespondingServiceBacksOffToMaximumDelay) {
  DisplayNotificationSync();
  EXPECT_TRUE(provider_factory_->is_service_connected());

  // Simulate a service (such as a background AppShim) that repeatedly times out
  // after 120 seconds (> 10s reset threshold) without ever responding to
  // OkayToTerminateService(). The restart delay should double on each attempt
  // (500ms, 1s, 2s, ..., 256s) rather than resetting to 500ms, and stay capped
  // at 256s.
  MockNotificationService::OkayToTerminateServiceCallback unanswered_callback;
  for (base::TimeDelta expected_delay = base::Milliseconds(500);
       expected_delay <= base::Seconds(256); expected_delay *= 2) {
    EXPECT_CALL(service(), OkayToTerminateService)
        .WillOnce(
            [&](MockNotificationService::OkayToTerminateServiceCallback cb) {
              unanswered_callback = std::move(cb);
            });
    task_environment_.FastForwardBy(base::Seconds(120));
    provider_factory_->Disconnect();
    unanswered_callback.Reset();
    EXPECT_FALSE(provider_factory_->is_service_connected());

    task_environment_.FastForwardBy(expected_delay - base::Milliseconds(1));
    EXPECT_FALSE(provider_factory_->is_service_connected());
    task_environment_.FastForwardBy(base::Milliseconds(1));
    EXPECT_TRUE(provider_factory_->is_service_connected());
    testing::Mock::VerifyAndClearExpectations(&service());
  }

  // Once the 256s cap is reached, another failure should still retry after
  // 256s.
  task_environment_.FastForwardBy(base::Seconds(120));
  provider_factory_->Disconnect();
  unanswered_callback.Reset();
  EXPECT_FALSE(provider_factory_->is_service_connected());

  EmulateOkayToTerminate(/*can_terminate=*/false);
  task_environment_.FastForwardBy(base::Seconds(256) - base::Milliseconds(1));
  EXPECT_FALSE(provider_factory_->is_service_connected());
  task_environment_.FastForwardBy(base::Milliseconds(1));
  EXPECT_TRUE(provider_factory_->is_service_connected());
}

class NotificationDispatcherMojoAppShimTest
    : public NotificationDispatcherMojoTest {
 public:
  NotificationDispatcherMojoAppShimTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kAppShimNotificationAttribution,
                              features::kUseAdHocSigningForWebAppShims},
        /*disabled_features=*/{});
  }

  void SetUp() override {
    NotificationDispatcherMojoTest::SetUp();

    auto provider_factory =
        std::make_unique<FakeMacNotificationProviderFactory>(
            app_shim_on_disconnect_.Get(),
            mac_notifications::NotificationStyle::kAppShim);
    app_shim_factory_ = provider_factory.get();

    base::test::TestFuture<void> startup_future;
    EXPECT_CALL(app_shim_service(), OkayToTerminateService)
        .WillOnce(
            [](MockNotificationService::OkayToTerminateServiceCallback cb) {
              std::move(cb).Run(/*can_terminate=*/true);
            });
    EXPECT_CALL(app_shim_on_disconnect_, Run())
        .WillOnce(base::test::RunOnceClosure(startup_future.GetCallback()))
        .WillRepeatedly(testing::DoDefault());
    app_shim_dispatcher_ = std::make_unique<NotificationDispatcherMojo>(
        std::move(provider_factory));
    ASSERT_TRUE(startup_future.Wait());
    testing::Mock::VerifyAndClearExpectations(&app_shim_service());
  }

  void TearDown() override {
    app_shim_factory_ = nullptr;
    app_shim_dispatcher_.reset();
    NotificationDispatcherMojoTest::TearDown();
  }

 protected:
  MockNotificationService& app_shim_service() {
    return app_shim_factory_->service();
  }

  void EmulateAppShimOkayToTerminate(bool can_terminate) {
    EXPECT_CALL(app_shim_service(), OkayToTerminateService)
        .WillRepeatedly(
            [can_terminate](
                MockNotificationService::OkayToTerminateServiceCallback cb) {
              std::move(cb).Run(can_terminate);
            });
  }

  void DisplayAppShimNotificationSync() {
    base::test::TestFuture<void> display_future;
    EXPECT_CALL(app_shim_service(), DisplayNotification)
        .WillOnce(base::test::RunOnceClosure(display_future.GetCallback()));
    app_shim_dispatcher_->DisplayNotification(
        NotificationHandler::Type::WEB_PERSISTENT, profile_,
        CreateNotification());
    EXPECT_TRUE(display_future.Wait());
    EXPECT_TRUE(app_shim_factory_->is_service_connected());
  }

  // Simulates an app shim that never responds and keeps timing out, until the
  // dispatcher stops scheduling automatic restarts. Returns the number of
  // automatic restarts that happened.
  int SimulateNeverRespondingAppShim() {
    constexpr int kMaxAttempts = 50;
    int restart_count = 0;
    std::vector<MockNotificationService::OkayToTerminateServiceCallback>
        unanswered_callbacks;
    EXPECT_CALL(app_shim_service(), OkayToTerminateService)
        .WillRepeatedly(
            [&](MockNotificationService::OkayToTerminateServiceCallback cb) {
              unanswered_callbacks.push_back(std::move(cb));
            });
    for (int attempt = 0; attempt < kMaxAttempts; ++attempt) {
      task_environment_.FastForwardBy(base::Seconds(120));
      app_shim_factory_->Disconnect();
      unanswered_callbacks.clear();
      EXPECT_FALSE(app_shim_factory_->is_service_connected());

      task_environment_.FastForwardBy(base::Hours(1));
      if (!app_shim_factory_->is_service_connected()) {
        break;
      }
      ++restart_count;
    }
    testing::Mock::VerifyAndClearExpectations(&app_shim_service());
    EXPECT_LT(restart_count, kMaxAttempts);
    return restart_count;
  }

  void ExpectCloseNotificationReachesAppShim() {
    base::test::TestFuture<void> close_future;
    EXPECT_CALL(app_shim_service(), CloseNotification)
        .WillOnce(base::test::RunOnceClosure(close_future.GetCallback()));
    app_shim_dispatcher_->CloseNotificationWithId(
        {kNotificationId, kProfileId, /*incognito=*/false});
    EXPECT_TRUE(close_future.Wait());
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  base::MockRepeatingClosure app_shim_on_disconnect_;
  raw_ptr<FakeMacNotificationProviderFactory> app_shim_factory_ = nullptr;
  std::unique_ptr<NotificationDispatcherMojo> app_shim_dispatcher_;
};

TEST_F(NotificationDispatcherMojoAppShimTest, StopsRetryingAfterMaximumDelay) {
  DisplayAppShimNotificationSync();

  // The dispatcher should retry at least once but eventually stop scheduling
  // automatic restarts.
  EXPECT_GT(SimulateNeverRespondingAppShim(), 0);
  EXPECT_FALSE(app_shim_factory_->is_service_connected());
}

TEST_F(NotificationDispatcherMojoAppShimTest,
       GetDisplayedNotificationsRelaunchesAfterGivingUp) {
  DisplayAppShimNotificationSync();
  SimulateNeverRespondingAppShim();

  // Notifications may still be on screen, so querying them should relaunch the
  // app shim rather than reporting that nothing is displayed.
  MockNotificationService::GetDisplayedNotificationsCallback
      unanswered_callback;
  EXPECT_CALL(app_shim_service(), GetDisplayedNotifications)
      .WillOnce(
          [&](mac_notifications::mojom::ProfileIdentifierPtr,
              const std::optional<GURL>&,
              MockNotificationService::GetDisplayedNotificationsCallback cb) {
            unanswered_callback = std::move(cb);
          });
  base::test::TestFuture<std::set<std::string>, bool> get_future;
  app_shim_dispatcher_->GetDisplayedNotificationsForProfileId(
      kProfileId, /*incognito=*/false, get_future.GetCallback());
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(app_shim_factory_->is_service_connected());
  testing::Mock::VerifyAndClearExpectations(&app_shim_service());

  // If the app shim still does not respond, the result must not be used to
  // synchronize notifications.
  app_shim_factory_->Disconnect();
  unanswered_callback.Reset();
  auto [notification_ids, supports_synchronization] = get_future.Take();
  EXPECT_TRUE(notification_ids.empty());
  EXPECT_FALSE(supports_synchronization);
}

TEST_F(NotificationDispatcherMojoAppShimTest,
       CloseNotificationRelaunchesAfterGivingUp) {
  DisplayAppShimNotificationSync();
  SimulateNeverRespondingAppShim();

  EmulateAppShimOkayToTerminate(/*can_terminate=*/true);
  ExpectCloseNotificationReachesAppShim();
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(app_shim_factory_->is_service_connected());
  testing::Mock::VerifyAndClearExpectations(&app_shim_service());

  // Now that the app shim reported it has no notifications left, queries should
  // be answered without relaunching it.
  base::test::TestFuture<std::set<std::string>, bool> get_future;
  app_shim_dispatcher_->GetDisplayedNotificationsForProfileId(
      kProfileId, /*incognito=*/false, get_future.GetCallback());
  auto [notification_ids, supports_synchronization] = get_future.Take();
  EXPECT_TRUE(notification_ids.empty());
  EXPECT_TRUE(supports_synchronization);
  EXPECT_FALSE(app_shim_factory_->is_service_connected());
}

TEST_F(NotificationDispatcherMojoAppShimTest,
       DisplayNotificationRelaunchesAfterGivingUp) {
  DisplayAppShimNotificationSync();
  SimulateNeverRespondingAppShim();

  DisplayAppShimNotificationSync();
  EmulateAppShimOkayToTerminate(/*can_terminate=*/false);
  ExpectCloseNotificationReachesAppShim();
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(app_shim_factory_->is_service_connected());
}

TEST_F(NotificationDispatcherMojoAppShimTest,
       CloseNotificationRelaunchesAfterUserInitiatedShutdown) {
  DisplayAppShimNotificationSync();

  // Simulate the user quitting the app shim.
  app_shim_dispatcher_->UserInitiatedShutdown();
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(app_shim_factory_->is_service_connected());

  // Notifications shown by the app shim may still be on screen, so closing one
  // should relaunch the app shim.
  EmulateAppShimOkayToTerminate(/*can_terminate=*/true);
  ExpectCloseNotificationReachesAppShim();
}

TEST_F(NotificationDispatcherMojoAppShimTest,
       UserInitiatedShutdownWithoutNotificationsDoesNotRelaunch) {
  // The app shim already reported it has no notifications during SetUp(), so
  // the user quitting it should not cause later calls to relaunch it.
  app_shim_dispatcher_->UserInitiatedShutdown();
  task_environment_.RunUntilIdle();

  EXPECT_CALL(app_shim_service(), CloseNotification).Times(0);
  app_shim_dispatcher_->CloseNotificationWithId(
      {kNotificationId, kProfileId, /*incognito=*/false});
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(app_shim_factory_->is_service_connected());

  base::test::TestFuture<std::set<std::string>, bool> get_future;
  app_shim_dispatcher_->GetDisplayedNotificationsForProfileId(
      kProfileId, /*incognito=*/false, get_future.GetCallback());
  auto [notification_ids, supports_synchronization] = get_future.Take();
  EXPECT_TRUE(notification_ids.empty());
  EXPECT_TRUE(supports_synchronization);
  EXPECT_FALSE(app_shim_factory_->is_service_connected());
}
