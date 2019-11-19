// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/assistant_notification_controller.h"

#include "ash/assistant/assistant_controller.h"
#include "ash/assistant/model/assistant_notification_model_observer.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/test/task_environment.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/services/assistant/public/mojom/assistant.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

using chromeos::assistant::mojom::AssistantNotificationPtr;
using testing::_;
using testing::Eq;
using testing::Field;
using testing::StrictMock;

constexpr bool kAnyBool = false;

class AssistantNotificationModelObserverMock
    : public AssistantNotificationModelObserver {
 public:
  AssistantNotificationModelObserverMock() = default;
  ~AssistantNotificationModelObserverMock() override = default;

  MOCK_METHOD(void,
              OnNotificationAdded,
              (const AssistantNotification* notification),
              (override));
  MOCK_METHOD(void,
              OnNotificationUpdated,
              (const AssistantNotification* notification),
              (override));
  MOCK_METHOD(void,
              OnNotificationRemoved,
              (const AssistantNotification* notification, bool from_server),
              (override));
  MOCK_METHOD(void, OnAllNotificationsRemoved, (bool from_server), (override));

 private:
  DISALLOW_COPY_AND_ASSIGN(AssistantNotificationModelObserverMock);
};

MATCHER_P(IdIs, expected_id, "") {
  if (arg->client_id != expected_id) {
    *result_listener << "Received notification with a wrong id.\n"
                     << "Expected:\n    '" << expected_id << "'\n"
                     << "Actual:\n    '" << arg->client_id << "'\n";
    return false;
  }
  return true;
}

class AssistantNotificationControllerTest : public AshTestBase {
 protected:
  AssistantNotificationControllerTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~AssistantNotificationControllerTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();

    controller_ =
        Shell::Get()->assistant_controller()->notification_controller();
    DCHECK(controller_);
  }

  AssistantNotificationController& controller() { return *controller_; }

  AssistantNotificationModelObserverMock& AddStrictObserverMock() {
    observer_ =
        std::make_unique<StrictMock<AssistantNotificationModelObserverMock>>();
    controller().AddModelObserver(observer_.get());
    return *observer_;
  }

  AssistantNotificationPtr CreateNotification(const std::string& id) {
    auto notification =
        chromeos::assistant::mojom::AssistantNotification::New();
    notification->client_id = id;
    return notification;
  }

  AssistantNotificationPtr CreateNotification(const std::string& id,
                                              int timeout_ms) {
    auto result = CreateNotification(id);
    result->expiry_time =
        base::Time::Now() + base::TimeDelta::FromMilliseconds(timeout_ms);
    return result;
  }

  void AddNotification(const std::string& id, int timeout_ms) {
    controller().AddOrUpdateNotification(CreateNotification(id, timeout_ms));
  }

  void AddNotification(const std::string& id) {
    controller().AddOrUpdateNotification(CreateNotification(id));
  }

  void UpdateNotification(const std::string& id, int timeout_ms) {
    controller().AddOrUpdateNotification(CreateNotification(id, timeout_ms));
  }

  void UpdateNotification(const std::string& id) {
    controller().AddOrUpdateNotification(CreateNotification(id));
  }

  void RemoveNotification(const std::string& id) {
    controller().RemoveNotificationById(id, kAnyBool);
  }

  void ForwardTimeInMs(int time_in_ms) {
    task_environment_->FastForwardBy(
        base::TimeDelta::FromMilliseconds(time_in_ms));
  }

 private:
  AssistantNotificationController* controller_;
  std::unique_ptr<AssistantNotificationModelObserverMock> observer_;

  DISALLOW_COPY_AND_ASSIGN(AssistantNotificationControllerTest);
};

TEST_F(AssistantNotificationControllerTest,
       ShouldInformObserverOfNewNotifications) {
  auto& observer = AddStrictObserverMock();

  EXPECT_CALL(observer, OnNotificationAdded(IdIs("id")));

  controller().AddOrUpdateNotification(CreateNotification("id"));
}

TEST_F(AssistantNotificationControllerTest,
       ShouldInformObserverOfUpdatedNotifications) {
  const auto notification = CreateNotification("id");
  controller().AddOrUpdateNotification(notification.Clone());
  auto& observer = AddStrictObserverMock();

  EXPECT_CALL(observer, OnNotificationUpdated(IdIs("id")));

  controller().AddOrUpdateNotification(notification.Clone());
}

TEST_F(AssistantNotificationControllerTest,
       ShouldInformObserverOfRemovedNotifications) {
  const auto notification = CreateNotification("id");
  controller().AddOrUpdateNotification(notification.Clone());
  constexpr bool from_server = kAnyBool;
  auto& observer = AddStrictObserverMock();

  EXPECT_CALL(observer, OnNotificationRemoved(IdIs("id"), Eq(from_server)));

  controller().RemoveNotificationById(notification->client_id, from_server);
}

TEST_F(AssistantNotificationControllerTest,
       ShouldInformObserverOfRemoveAllNotifications) {
  const auto notification = CreateNotification("id");
  controller().AddOrUpdateNotification(notification.Clone());
  constexpr bool from_server = !kAnyBool;
  auto& observer = AddStrictObserverMock();

  EXPECT_CALL(observer, OnAllNotificationsRemoved(Eq(from_server)));

  controller().RemoveAllNotifications(from_server);
}

TEST_F(AssistantNotificationControllerTest,
       ShouldRemoveNotificationWhenItExpires) {
  constexpr int timeout_ms = 1000;
  AddNotification("id", timeout_ms);
  auto& observer = AddStrictObserverMock();

  EXPECT_CALL(observer, OnNotificationRemoved(IdIs("id"), _));

  ForwardTimeInMs(timeout_ms);
}

TEST_F(AssistantNotificationControllerTest,
       ShouldNotRemoveNotificationsTooSoon) {
  constexpr int timeout_ms = 1000;
  AddNotification("id", timeout_ms);
  auto& observer = AddStrictObserverMock();

  EXPECT_CALL(observer, OnNotificationRemoved).Times(0);
  ForwardTimeInMs(timeout_ms - 1);
}

TEST_F(AssistantNotificationControllerTest,
       ShouldUseFromServerFalseWhenNotificationExpires) {
  constexpr int timeout_ms = 1000;
  AddNotification("id", timeout_ms);
  auto& observer = AddStrictObserverMock();

  EXPECT_CALL(observer, OnNotificationRemoved(_, Eq(false)));

  ForwardTimeInMs(timeout_ms);
}

TEST_F(AssistantNotificationControllerTest,
       ShouldRemoveEachNotificationAsItExpires) {
  constexpr int first_timeout_ms = 1000;
  constexpr int second_timeout_ms = 1500;

  AddNotification("first", first_timeout_ms);
  AddNotification("second", second_timeout_ms);

  auto& observer = AddStrictObserverMock();

  EXPECT_CALL(observer, OnNotificationRemoved(IdIs("first"), _));
  ForwardTimeInMs(first_timeout_ms);

  EXPECT_CALL(observer, OnNotificationRemoved(IdIs("second"), _));
  int delta_between_notifications = second_timeout_ms - first_timeout_ms;
  ForwardTimeInMs(delta_between_notifications);
}

TEST_F(AssistantNotificationControllerTest,
       ShouldSupport2NotificationsThatExpireAtTheSameTime) {
  constexpr int timeout_ms = 1000;

  AddNotification("first", timeout_ms);
  AddNotification("at-same-time", timeout_ms);

  auto& observer = AddStrictObserverMock();

  EXPECT_CALL(observer, OnNotificationRemoved(IdIs("first"), _));
  EXPECT_CALL(observer, OnNotificationRemoved(IdIs("at-same-time"), _));
  ForwardTimeInMs(timeout_ms);
}

TEST_F(AssistantNotificationControllerTest,
       ShouldImmediateRemoveNotificationsThatAlreadyExpired) {
  constexpr int negative_timeout = -1000;

  AddNotification("expired", negative_timeout);

  auto& observer = AddStrictObserverMock();

  EXPECT_CALL(observer, OnNotificationRemoved(IdIs("expired"), _));
}

TEST_F(AssistantNotificationControllerTest,
       ShouldNotRemoveNotificationsThatWereManuallyRemoved) {
  constexpr int timeout = 1000;

  AddNotification("id", timeout);
  RemoveNotification("id");

  auto& observer = AddStrictObserverMock();

  EXPECT_CALL(observer, OnNotificationRemoved).Times(0);
  ForwardTimeInMs(timeout);
}

TEST_F(AssistantNotificationControllerTest,
       ShouldSupportExpiryTimeSetInUpdate) {
  constexpr int timeout = 1000;

  AddNotification("id");
  UpdateNotification("id", timeout);

  auto& observer = AddStrictObserverMock();

  EXPECT_CALL(observer, OnNotificationRemoved);
  ForwardTimeInMs(timeout);
}

TEST_F(AssistantNotificationControllerTest,
       ShouldNotRemoveNotificationIfExpiryTimeIsClearedInUpdate) {
  constexpr int timeout = 1000;

  AddNotification("id", timeout);
  UpdateNotification("id");

  auto& observer = AddStrictObserverMock();

  EXPECT_CALL(observer, OnNotificationRemoved).Times(0);
  ForwardTimeInMs(timeout);
}

}  // namespace ash
