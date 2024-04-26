// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/external_arc/message_center/arc_notification_manager.h"

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/components/arc/session/connection_holder.h"
#include "ash/components/arc/test/connection_holder_util.h"
#include "ash/components/arc/test/fake_notifications_instance.h"
#include "ash/public/cpp/arc_app_id_provider.h"
#include "ash/public/cpp/message_center/arc_notification_manager_delegate.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/fake_message_center.h"
#include "ui/message_center/message_center_observer.h"

namespace ash {

namespace {

constexpr char kDummyNotificationKey[] = "DUMMY_NOTIFICATION_KEY";
constexpr char kHistogramNameActionEnabled[] =
    "Arc.Notifications.ActionEnabled";
constexpr char kHistogramNameStyle[] = "Arc.Notifications.Style";
constexpr char kHistogramNameInlineReplyEnabled[] =
    "Arc.Notifications.InlineReplyEnabled";
constexpr char kHistogramNameIsCustomNotification[] =
    "Arc.Notifications.IsCustomNotification";

class TestArcAppIdProvider : public ArcAppIdProvider {
 public:
  TestArcAppIdProvider() = default;

  TestArcAppIdProvider(const TestArcAppIdProvider&) = delete;
  TestArcAppIdProvider& operator=(const TestArcAppIdProvider&) = delete;

  ~TestArcAppIdProvider() override = default;

  // ArcAppIdProvider:
  std::string GetAppIdByPackageName(const std::string& package_name) override {
    return {};
  }
};

class MockMessageCenter : public message_center::FakeMessageCenter {
 public:
  MockMessageCenter() = default;

  MockMessageCenter(const MockMessageCenter&) = delete;
  MockMessageCenter& operator=(const MockMessageCenter&) = delete;

  ~MockMessageCenter() override = default;

  void AddNotification(
      std::unique_ptr<message_center::Notification> notification) override {
    visible_notifications_.insert(notification.get());
    std::string id = notification->id();
    owned_notifications_[id] = std::move(notification);

    if (on_added_notification_callback_) {
      on_added_notification_callback_.Run();
    }
  }

  void RemoveNotification(const std::string& id, bool by_user) override {
    auto it = owned_notifications_.find(id);
    if (it == owned_notifications_.end())
      return;

    visible_notifications_.erase(it->second.get());
    owned_notifications_.erase(it);
  }

  const message_center::NotificationList::Notifications&
  GetVisibleNotifications() override {
    return visible_notifications_;
  }

  void SetQuietMode(
      bool in_quiet_mode,
      message_center::QuietModeSourceType type =
          message_center::QuietModeSourceType::kUserAction) override {
    if (in_quiet_mode != in_quiet_mode_) {
      in_quiet_mode_ = in_quiet_mode;
      for (auto& observer : observer_list())
        observer.OnQuietModeChanged(in_quiet_mode);
    }
  }

  bool IsQuietMode() const override { return in_quiet_mode_; }

  void SetOnAddedNotificationCallback(base::RepeatingClosure callback) {
    on_added_notification_callback_ = callback;
  }

 private:
  message_center::NotificationList::Notifications visible_notifications_;
  std::map<std::string, std::unique_ptr<message_center::Notification>>
      owned_notifications_;
  bool in_quiet_mode_ = false;
  base::RepeatingClosure on_added_notification_callback_;
};

class FakeArcNotificationManagerDelegate
    : public ArcNotificationManagerDelegate {
 public:
  FakeArcNotificationManagerDelegate() = default;

  FakeArcNotificationManagerDelegate(
      const FakeArcNotificationManagerDelegate&) = delete;
  FakeArcNotificationManagerDelegate& operator=(
      const FakeArcNotificationManagerDelegate&) = delete;

  ~FakeArcNotificationManagerDelegate() override = default;

  // ArcNotificationManagerDelegate:
  bool IsManagedGuestSessionOrKiosk() const override { return false; }
  void ShowMessageCenter() override {}
  void HideMessageCenter() override {}
};

class FakeObserver : public ArcNotificationManagerBase::Observer {
 public:
  FakeObserver() = default;
  ~FakeObserver() override = default;

  void OnNotificationUpdated(const std::string& notification_id,
                             const std::string& app_id) override {}
  void OnNotificationRemoved(const std::string& notification_id) override {}
  void OnArcNotificationManagerDestroyed(
      ArcNotificationManagerBase* manager) override {}
};

}  // anonymous namespace

class ArcNotificationManagerTest : public testing::Test {
 public:
  ArcNotificationManagerTest() = default;

  ArcNotificationManagerTest(const ArcNotificationManagerTest&) = delete;
  ArcNotificationManagerTest& operator=(const ArcNotificationManagerTest&) =
      delete;

  ~ArcNotificationManagerTest() override = default;

 protected:
  arc::FakeNotificationsInstance* arc_notifications_instance() {
    return arc_notifications_instance_.get();
  }
  ArcNotificationManager* arc_notification_manager() {
    return arc_notification_manager_.get();
  }
  MockMessageCenter* message_center() { return message_center_.get(); }

  std::string CreateNotification() {
    return CreateNotificationWithKey(kDummyNotificationKey);
  }

  std::string CreateNotificationWithKey(const std::string& key) {
    auto data = arc::mojom::ArcNotificationData::New();
    data->key = key;
    data->title = "TITLE";
    data->message = "MESSAGE";
    data->package_name = "PACKAGE_NAME";

    arc_notification_manager()->OnNotificationPosted(std::move(data));

    return key;
  }

  void FlushInstanceCall() { receiver_->FlushForTesting(); }

  void ConnectMojoChannel() {
    receiver_ =
        std::make_unique<mojo::Receiver<arc::mojom::NotificationsInstance>>(
            arc_notifications_instance_.get());
    mojo::PendingRemote<arc::mojom::NotificationsInstance> remote;
    receiver_->Bind(remote.InitWithNewPipeAndPassReceiver());

    arc_notification_manager_->SetInstance(std::move(remote));
    WaitForInstanceReady(
        arc_notification_manager_->GetConnectionHolderForTest());
  }

 private:
  void SetUp() override {
    arc_notifications_instance_ =
        std::make_unique<arc::FakeNotificationsInstance>();
    message_center_ = std::make_unique<MockMessageCenter>();

    arc_notification_manager_ = std::make_unique<ArcNotificationManager>();
    arc_notification_manager_->Init(
        std::make_unique<FakeArcNotificationManagerDelegate>(),
        EmptyAccountId(), message_center_.get());
  }

  void TearDown() override {
    arc_notification_manager_.reset();
    message_center_.reset();
    receiver_.reset();
    arc_notifications_instance_.reset();
    base::RunLoop().RunUntilIdle();
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  TestArcAppIdProvider app_id_provider_;
  std::unique_ptr<arc::FakeNotificationsInstance> arc_notifications_instance_;
  std::unique_ptr<mojo::Receiver<arc::mojom::NotificationsInstance>> receiver_;
  std::unique_ptr<ArcNotificationManager> arc_notification_manager_;
  std::unique_ptr<MockMessageCenter> message_center_;
};

TEST_F(ArcNotificationManagerTest, NotificationCreatedAndRemoved) {
  EXPECT_EQ(0u, message_center()->GetVisibleNotifications().size());
  std::string key = CreateNotification();
  EXPECT_EQ(1u, message_center()->GetVisibleNotifications().size());

  arc_notification_manager()->OnNotificationRemoved(key);

  EXPECT_EQ(0u, message_center()->GetVisibleNotifications().size());
}

TEST_F(ArcNotificationManagerTest, NotificationRemovedByChrome) {
  ConnectMojoChannel();

  EXPECT_EQ(0u, message_center()->GetVisibleNotifications().size());
  std::string key = CreateNotification();
  EXPECT_EQ(1u, message_center()->GetVisibleNotifications().size());
  {
    message_center::Notification* notification =
        *message_center()->GetVisibleNotifications().begin();
    notification->delegate()->Close(true /* by_user */);
    // |notification| gets stale here.
  }

  FlushInstanceCall();

  ASSERT_EQ(1u, arc_notifications_instance()->events().size());
  EXPECT_EQ(key, arc_notifications_instance()->events().at(0).first);
  EXPECT_EQ(arc::mojom::ArcNotificationEvent::CLOSED,
            arc_notifications_instance()->events().at(0).second);
}

TEST_F(ArcNotificationManagerTest, NotificationRemovedByConnectionClose) {
  ConnectMojoChannel();

  EXPECT_EQ(0u, message_center()->GetVisibleNotifications().size());
  CreateNotificationWithKey("notification1");
  CreateNotificationWithKey("notification2");
  CreateNotificationWithKey("notification3");
  EXPECT_EQ(3u, message_center()->GetVisibleNotifications().size());

  arc_notification_manager()->OnConnectionClosed();

  EXPECT_EQ(0u, message_center()->GetVisibleNotifications().size());
}

TEST_F(ArcNotificationManagerTest, DoNotDisturbSetStatusByRequestFromAndroid) {
  ConnectMojoChannel();

  // Check the initial conditions.
  EXPECT_FALSE(message_center()->IsQuietMode());
  EXPECT_TRUE(
      arc_notifications_instance()->latest_do_not_disturb_status().is_null());

  // Emulate the request from Android to turn on Do-Not-Distturb.
  arc::mojom::ArcDoNotDisturbStatusPtr status =
      arc::mojom::ArcDoNotDisturbStatus::New();
  status->enabled = true;
  arc_notification_manager()->OnDoNotDisturbStatusUpdated(std::move(status));
  // Confirm that the Do-Not-Disturb is on.
  EXPECT_TRUE(message_center()->IsQuietMode());
  // Confirm that no message back to Android.
  EXPECT_TRUE(
      arc_notifications_instance()->latest_do_not_disturb_status().is_null());

  // Emulate the request from Android to turn off Do-Not-Disturb.
  status = arc::mojom::ArcDoNotDisturbStatus::New();
  status->enabled = false;
  arc_notification_manager()->OnDoNotDisturbStatusUpdated(std::move(status));
  // Confirm that the Do-Not-Disturb is off.
  EXPECT_FALSE(message_center()->IsQuietMode());
  // Confirm that no message back to Android.
  EXPECT_TRUE(
      arc_notifications_instance()->latest_do_not_disturb_status().is_null());
}

TEST_F(ArcNotificationManagerTest, DoNotDisturbSendStatusToAndroid) {
  ConnectMojoChannel();
  // FlushInstanceCall();

  // Check the initial conditions.
  EXPECT_TRUE(
      arc_notifications_instance()->latest_do_not_disturb_status().is_null());
  EXPECT_FALSE(message_center()->IsQuietMode());

  // Trying setting the current value (false). This should be no-op
  message_center()->SetQuietMode(false);
  // A request to Android should not be sent, since the status is not changed.
  EXPECT_TRUE(
      arc_notifications_instance()->latest_do_not_disturb_status().is_null());

  // Trying turning on.
  message_center()->SetQuietMode(true);
  FlushInstanceCall();
  // base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(
      arc_notifications_instance()->latest_do_not_disturb_status().is_null());
  // A request to Android should be sent.
  EXPECT_TRUE(
      arc_notifications_instance()->latest_do_not_disturb_status()->enabled);
}

TEST_F(ArcNotificationManagerTest, DoNotDisturbSyncInitialDisabledState) {
  // Check the initial conditions.
  EXPECT_TRUE(
      arc_notifications_instance()->latest_do_not_disturb_status().is_null());
  EXPECT_FALSE(message_center()->IsQuietMode());

  // Establish the mojo connection.
  ConnectMojoChannel();
  FlushInstanceCall();
  // The request to Android should be sent, and the status should be synced
  // accordingly.
  EXPECT_FALSE(
      arc_notifications_instance()->latest_do_not_disturb_status().is_null());
  EXPECT_FALSE(
      arc_notifications_instance()->latest_do_not_disturb_status()->enabled);
}

TEST_F(ArcNotificationManagerTest, DoNotDisturbSyncInitialEnabledState) {
  // Set quiet mode.
  message_center()->SetQuietMode(true);
  // Check the initial condition.
  EXPECT_TRUE(message_center()->IsQuietMode());
  EXPECT_TRUE(
      arc_notifications_instance()->latest_do_not_disturb_status().is_null());

  // Establish the mojo connection.
  ConnectMojoChannel();
  FlushInstanceCall();
  // The request to Android should be sent, and the status should be synced
  // accordingly.
  EXPECT_FALSE(
      arc_notifications_instance()->latest_do_not_disturb_status().is_null());
  EXPECT_TRUE(
      arc_notifications_instance()->latest_do_not_disturb_status()->enabled);
}

TEST_F(ArcNotificationManagerTest,
       UmaMeticsPublishedOnlyWhenNotificationCreated) {
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(kHistogramNameActionEnabled, 0);
  histogram_tester.ExpectTotalCount(kHistogramNameStyle, 0);
  histogram_tester.ExpectTotalCount(kHistogramNameInlineReplyEnabled, 0);
  histogram_tester.ExpectTotalCount(kHistogramNameIsCustomNotification, 0);

  // Create notification
  std::string key = CreateNotification();
  histogram_tester.ExpectTotalCount(kHistogramNameActionEnabled, 1);
  histogram_tester.ExpectTotalCount(kHistogramNameStyle, 1);
  histogram_tester.ExpectTotalCount(kHistogramNameInlineReplyEnabled, 1);
  histogram_tester.ExpectTotalCount(kHistogramNameIsCustomNotification, 1);

  // Update notification
  CreateNotificationWithKey(key);
  histogram_tester.ExpectTotalCount(kHistogramNameActionEnabled, 1);
  histogram_tester.ExpectTotalCount(kHistogramNameStyle, 1);
  histogram_tester.ExpectTotalCount(kHistogramNameInlineReplyEnabled, 1);
  histogram_tester.ExpectTotalCount(kHistogramNameIsCustomNotification, 1);
}

TEST_F(ArcNotificationManagerTest, NotificationRemovedWhileAddingOne) {
  ConnectMojoChannel();

  // Create and remove enough number of notifications causing relocation in the
  // map.
  CreateNotificationWithKey("notification1");
  EXPECT_EQ(1u, message_center()->GetVisibleNotifications().size());

  FakeObserver observer;
  arc_notification_manager()->AddObserver(&observer);

  // Set the callback to remove added notification immediately
  message_center()->SetOnAddedNotificationCallback(base::BindLambdaForTesting(
      [arc_notification_manager = arc_notification_manager()]() {
        arc_notification_manager->SendNotificationRemovedFromChrome(
            "notification2");
      }));

  // It should not crash.
  CreateNotificationWithKey("notification2");

  arc_notification_manager()->RemoveObserver(&observer);
}

}  // namespace ash
