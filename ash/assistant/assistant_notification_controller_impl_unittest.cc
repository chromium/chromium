// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/assistant_notification_controller_impl.h"

#include <map>
#include <memory>
#include <utility>

#include "ash/assistant/assistant_controller_impl.h"
#include "ash/assistant/model/assistant_notification_model_observer.h"
#include "ash/assistant/test/assistant_ash_test_base.h"
#include "ash/assistant/test/test_assistant_service.h"
#include "ash/shell.h"
#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/services/assistant/public/cpp/assistant_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/message_center.h"

namespace ash {

namespace {

using assistant::AssistantNotification;
using assistant::AssistantNotificationButton;
using assistant::AssistantNotificationPriority;

using testing::_;
using testing::Eq;
using testing::Field;
using testing::Mock;
using testing::StrictMock;

// Constants.
constexpr bool kAnyBool = false;

// Matchers --------------------------------------------------------------------

MATCHER_P(IdIs, expected_id, "") {
  if (arg.client_id != expected_id) {
    *result_listener << "Received notification with a wrong id.\n"
                     << "Expected:\n    '" << expected_id << "'\n"
                     << "Actual:\n    '" << arg.client_id << "'\n";
    return false;
  }
  return true;
}

// Builders --------------------------------------------------------------------

class AssistantNotificationBuilder {
 public:
  AssistantNotificationBuilder() = default;

  AssistantNotificationBuilder(const AssistantNotificationBuilder& bldr) {
    notification_ = bldr.notification_;
  }

  ~AssistantNotificationBuilder() = default;

  AssistantNotification Build() const { return notification_; }

  AssistantNotificationBuilder& WithId(const std::string& id) {
    notification_.client_id = id;
    notification_.server_id = id;
    return *this;
  }

  AssistantNotificationBuilder& WithActionUrl(const GURL& action_url) {
    notification_.action_url = action_url;
    return *this;
  }

  AssistantNotificationBuilder& WithButton(AssistantNotificationButton&& button,
                                           int index = -1) {
    if (index != -1) {
      notification_.buttons.insert(notification_.buttons.begin() + index,
                                   std::move(button));
    } else {
      notification_.buttons.push_back(std::move(button));
    }
    return *this;
  }

  AssistantNotificationBuilder& WithFromServer(bool from_server) {
    notification_.from_server = from_server;
    return *this;
  }

  AssistantNotificationBuilder& WithIsPinned(bool is_pinned) {
    notification_.is_pinned = is_pinned;
    return *this;
  }

  AssistantNotificationBuilder& WithPriority(
      AssistantNotificationPriority priority) {
    notification_.priority = priority;
    return *this;
  }

  AssistantNotificationBuilder& WithRemoveOnClick(bool remove_on_click) {
    notification_.remove_on_click = remove_on_click;
    return *this;
  }

  AssistantNotificationBuilder& WithRenotify(bool renotify) {
    notification_.renotify = renotify;
    return *this;
  }

  AssistantNotificationBuilder& WithTimeout(
      std::optional<base::TimeDelta> timeout) {
    notification_.expiry_time =
        timeout.has_value()
            ? std::optional<base::Time>(base::Time::Now() + timeout.value())
            : std::nullopt;
    return *this;
  }

  AssistantNotificationBuilder& WithTimeoutMs(int timeout_ms) {
    return WithTimeout(base::Milliseconds(timeout_ms));
  }

 private:
  AssistantNotification notification_;
};

class AssistantNotificationButtonBuilder {
 public:
  AssistantNotificationButtonBuilder() = default;

  AssistantNotificationButtonBuilder(
      const AssistantNotificationButtonBuilder& bldr) {
    button_ = bldr.button_;
  }

  ~AssistantNotificationButtonBuilder() = default;

  AssistantNotificationButton Build() const { return button_; }

  AssistantNotificationButtonBuilder& WithLabel(const std::string& label) {
    button_.label = label;
    return *this;
  }

  AssistantNotificationButtonBuilder& WithActionUrl(const GURL& action_url) {
    button_.action_url = action_url;
    return *this;
  }

  AssistantNotificationButtonBuilder& WithRemoveNotificationOnClick(
      bool remove_notification_on_click) {
    button_.remove_notification_on_click = remove_notification_on_click;
    return *this;
  }

 private:
  AssistantNotificationButton button_;
};

// Mocks -----------------------------------------------------------------------

class AssistantNotificationModelObserverMock
    : public AssistantNotificationModelObserver {
 public:
  AssistantNotificationModelObserverMock() = default;

  AssistantNotificationModelObserverMock(
      const AssistantNotificationModelObserverMock&) = delete;
  AssistantNotificationModelObserverMock& operator=(
      const AssistantNotificationModelObserverMock&) = delete;

  ~AssistantNotificationModelObserverMock() override = default;

  MOCK_METHOD(void,
              OnNotificationAdded,
              (const AssistantNotification& notification),
              (override));
  MOCK_METHOD(void,
              OnNotificationUpdated,
              (const AssistantNotification& notification),
              (override));
  MOCK_METHOD(void,
              OnNotificationRemoved,
              (const AssistantNotification& notification, bool from_server),
              (override));
  MOCK_METHOD(void, OnAllNotificationsRemoved, (bool from_server), (override));
};

class AssistantServiceMock : public TestAssistantService {
 public:
  MOCK_METHOD(void,
              RetrieveNotification,
              (const AssistantNotification& notification, int action_index),
              (override));
};

// AssistantNotificationControllerTest -----------------------------------------

class AssistantNotificationControllerTest : public AssistantAshTestBase {
 public:
  AssistantNotificationControllerTest(
      const AssistantNotificationControllerTest&) = delete;
  AssistantNotificationControllerTest& operator=(
      const AssistantNotificationControllerTest&) = delete;

 protected:
  AssistantNotificationControllerTest()
      : AssistantAshTestBase(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~AssistantNotificationControllerTest() override = default;

  void SetUp() override {
    AssistantAshTestBase::SetUp();

    controller_ =
        Shell::Get()->assistant_controller()->notification_controller();
    DCHECK(controller_);
  }

  AssistantNotificationControllerImpl& controller() { return *controller_; }

  AssistantNotificationModelObserverMock& AddStrictObserverMock() {
    observer_ =
        std::make_unique<StrictMock<AssistantNotificationModelObserverMock>>();
    controller().model()->AddObserver(observer_.get());
    return *observer_;
  }

  void AddOrUpdateNotification(AssistantNotification&& notification) {
    controller().AddOrUpdateNotification(std::move(notification));
  }

  void RemoveNotification(const std::string& id) {
    controller().RemoveNotificationById(id, kAnyBool);
  }

  void ForwardTimeInMs(int time_in_ms) {
    task_environment()->FastForwardBy(base::Milliseconds(time_in_ms));
  }

 private:
  raw_ptr<AssistantNotificationControllerImpl, DanglingUntriaged> controller_;
  std::unique_ptr<AssistantNotificationModelObserverMock> observer_;
};

}  // namespace

// Tests -----------------------------------------------------------------------

TEST_F(AssistantNotificationControllerTest,
       ShouldInformObserverOfNewNotifications) {
  auto& observer = AddStrictObserverMock();

  EXPECT_CALL(observer, OnNotificationAdded(IdIs("id")));
  controller().AddOrUpdateNotification(
      AssistantNotificationBuilder().WithId("id").Build());
}

TEST_F(AssistantNotificationControllerTest,
       ShouldInformObserverOfUpdatedNotifications) {
  const auto builder = AssistantNotificationBuilder().WithId("id");
  controller().AddOrUpdateNotification(builder.Build());

  auto& observer = AddStrictObserverMock();

  EXPECT_CALL(observer, OnNotificationUpdated(IdIs("id")));
  controller().AddOrUpdateNotification(builder.Build());
}

TEST_F(AssistantNotificationControllerTest,
       ShouldInformObserverOfRemovedNotifications) {
  constexpr bool kFromServer = kAnyBool;

  auto builder = AssistantNotificationBuilder().WithId("id");
  controller().AddOrUpdateNotification(builder.Build());

  auto& observer = AddStrictObserverMock();

  EXPECT_CALL(observer, OnNotificationRemoved(IdIs("id"), Eq(kFromServer)));
  controller().RemoveNotificationById(builder.Build().client_id, kFromServer);
}

TEST_F(AssistantNotificationControllerTest,
       ShouldInformObserverOfRemoveAllNotifications) {
  constexpr bool kFromServer = !kAnyBool;

  controller().AddOrUpdateNotification(
      AssistantNotificationBuilder().WithId("id").Build());

  auto& observer = AddStrictObserverMock();

  EXPECT_CALL(observer, OnAllNotificationsRemoved(Eq(kFromServer)));
  controller().RemoveAllNotifications(kFromServer);
}

TEST_F(AssistantNotificationControllerTest,
       ShouldRemoveNotificationWhenItExpires) {
  constexpr int kTimeoutMs = 1000;

  AddOrUpdateNotification(AssistantNotificationBuilder()
                              .WithId("id")
                              .WithTimeoutMs((kTimeoutMs))
                              .Build());

  auto& observer = AddStrictObserverMock();

  EXPECT_CALL(observer, OnNotificationRemoved(IdIs("id"), _));
  ForwardTimeInMs(kTimeoutMs);
}

TEST_F(AssistantNotificationControllerTest,
       ShouldNotRemoveNotificationsTooSoon) {
  constexpr int kTimeoutMs = 1000;

  AddOrUpdateNotification(AssistantNotificationBuilder()
                              .WithId("id")
                              .WithTimeoutMs(kTimeoutMs)
                              .Build());

  auto& observer = AddStrictObserverMock();

  EXPECT_CALL(observer, OnNotificationRemoved).Times(0);
  ForwardTimeInMs(kTimeoutMs - 1);
}

TEST_F(AssistantNotificationControllerTest,
       ShouldUseFromServerFalseWhenNotificationExpires) {
  constexpr int kTimeoutMs = 1000;

  AddOrUpdateNotification(AssistantNotificationBuilder()
                              .WithId("id")
                              .WithTimeoutMs(kTimeoutMs)
                              .Build());

  auto& observer = AddStrictObserverMock();

  EXPECT_CALL(observer, OnNotificationRemoved(_, Eq(false)));
  ForwardTimeInMs(kTimeoutMs);
}

TEST_F(AssistantNotificationControllerTest,
       ShouldRemoveEachNotificationAsItExpires) {
  constexpr int kFirstTimeoutMs = 1000;
  constexpr int kSecondTimeoutMs = 1500;

  AddOrUpdateNotification(AssistantNotificationBuilder()
                              .WithId("first")
                              .WithTimeoutMs(kFirstTimeoutMs)
                              .Build());
  AddOrUpdateNotification(AssistantNotificationBuilder()
                              .WithId("second")
                              .WithTimeoutMs(kSecondTimeoutMs)
                              .Build());

  auto& observer = AddStrictObserverMock();

  EXPECT_CALL(observer, OnNotificationRemoved(IdIs("first"), _));
  ForwardTimeInMs(kFirstTimeoutMs);

  EXPECT_CALL(observer, OnNotificationRemoved(IdIs("second"), _));
  ForwardTimeInMs(kSecondTimeoutMs - kFirstTimeoutMs);
}

TEST_F(AssistantNotificationControllerTest,
       ShouldSupport2NotificationsThatExpireAtTheSameTime) {
  constexpr int kTimeoutMs = 1000;

  AddOrUpdateNotification(AssistantNotificationBuilder()
                              .WithId("first")
                              .WithTimeoutMs(kTimeoutMs)
                              .Build());
  AddOrUpdateNotification(AssistantNotificationBuilder()
                              .WithId("at-same-time")
                              .WithTimeoutMs(kTimeoutMs)
                              .Build());

  auto& observer = AddStrictObserverMock();

  EXPECT_CALL(observer, OnNotificationRemoved(IdIs("first"), _));
  EXPECT_CALL(observer, OnNotificationRemoved(IdIs("at-same-time"), _));
  ForwardTimeInMs(kTimeoutMs);
}

TEST_F(AssistantNotificationControllerTest,
       ShouldImmediateRemoveNotificationsThatAlreadyExpired) {
  constexpr int kNegativeTimeoutMs = -1000;

  AddOrUpdateNotification(AssistantNotificationBuilder()
                              .WithId("expired")
                              .WithTimeoutMs(kNegativeTimeoutMs)
                              .Build());

  auto& observer = AddStrictObserverMock();

  EXPECT_CALL(observer, OnNotificationRemoved(IdIs("expired"), _));
}

TEST_F(AssistantNotificationControllerTest,
       ShouldNotRemoveNotificationsThatWereManuallyRemoved) {
  constexpr int kTimeoutMs = 1000;

  AddOrUpdateNotification(AssistantNotificationBuilder()
                              .WithId("id")
                              .WithTimeoutMs(kTimeoutMs)
                              .Build());
  RemoveNotification("id");

  auto& observer = AddStrictObserverMock();

  EXPECT_CALL(observer, OnNotificationRemoved).Times(0);
  ForwardTimeInMs(kTimeoutMs);
}

TEST_F(AssistantNotificationControllerTest,
       ShouldSupportExpiryTimeSetInUpdate) {
  constexpr int kTimeoutMs = 1000;

  auto notification_bldr = AssistantNotificationBuilder().WithId("id");

  AddOrUpdateNotification(notification_bldr.Build());
  AddOrUpdateNotification(notification_bldr.WithTimeoutMs(kTimeoutMs).Build());

  auto& observer = AddStrictObserverMock();

  EXPECT_CALL(observer, OnNotificationRemoved);
  ForwardTimeInMs(kTimeoutMs);
}

TEST_F(AssistantNotificationControllerTest,
       ShouldNotRemoveNotificationIfExpiryTimeIsClearedInUpdate) {
  constexpr int kTimeoutMs = 1000;

  auto notification_bldr = AssistantNotificationBuilder().WithId("id");

  AddOrUpdateNotification(notification_bldr.WithTimeoutMs(kTimeoutMs).Build());
  AddOrUpdateNotification(notification_bldr.WithTimeout(std::nullopt).Build());

  auto& observer = AddStrictObserverMock();

  EXPECT_CALL(observer, OnNotificationRemoved).Times(0);
  ForwardTimeInMs(kTimeoutMs);
}

TEST_F(AssistantNotificationControllerTest,
       ShouldMaybeRemoveNotificationWhenClicking) {
  constexpr char kId[] = "id";

  auto notification_bldr =
      AssistantNotificationBuilder().WithId(kId).WithActionUrl(
          GURL("https://g.co/"));

  AddOrUpdateNotification(notification_bldr.WithRemoveOnClick(false).Build());

  auto& observer = AddStrictObserverMock();

  auto* message_center = message_center::MessageCenter::Get();

  EXPECT_CALL(observer, OnNotificationRemoved).Times(0);
  message_center->ClickOnNotification(kId);

  Mock::VerifyAndClearExpectations(&observer);

  EXPECT_CALL(observer, OnNotificationUpdated(IdIs(kId)));
  AddOrUpdateNotification(notification_bldr.WithRemoveOnClick(true).Build());

  EXPECT_CALL(observer, OnNotificationRemoved(IdIs(kId), _));
  message_center->ClickOnNotification(kId);
}

TEST_F(AssistantNotificationControllerTest,
       ShouldMaybeRemoveNotificationWhenClickingButton) {
  constexpr char kId[] = "id";

  auto notification_bldr = AssistantNotificationBuilder().WithId(kId);
  auto button_bldr =
      AssistantNotificationButtonBuilder().WithActionUrl(GURL("https://g.co/"));

  AddOrUpdateNotification(
      notification_bldr
          .WithButton(button_bldr.WithRemoveNotificationOnClick(false).Build())
          .Build());

  auto& observer = AddStrictObserverMock();

  auto* message_center = message_center::MessageCenter::Get();

  EXPECT_CALL(observer, OnNotificationRemoved).Times(0);
  message_center->ClickOnNotificationButton(kId, /*index=*/0);

  Mock::VerifyAndClearExpectations(&observer);

  EXPECT_CALL(observer, OnNotificationUpdated(IdIs(kId)));
  AddOrUpdateNotification(
      notification_bldr
          .WithButton(button_bldr.WithRemoveNotificationOnClick(true).Build(),
                      /*index=*/0)
          .Build());

  EXPECT_CALL(observer, OnNotificationRemoved(IdIs(kId), _));
  message_center->ClickOnNotificationButton(kId, /*index=*/0);
}

TEST_F(AssistantNotificationControllerTest,
       ShouldCorrectlyMapNotificationPriority) {
  constexpr char kId[] = "id";

  // Map of Assistant notification priorities to system notification priorities.
  const std::map<AssistantNotificationPriority,
                 message_center::NotificationPriority>
      priority_map = {{AssistantNotificationPriority::kLow,
                       message_center::NotificationPriority::LOW_PRIORITY},
                      {AssistantNotificationPriority::kDefault,
                       message_center::NotificationPriority::DEFAULT_PRIORITY},
                      {AssistantNotificationPriority::kHigh,
                       message_center::NotificationPriority::HIGH_PRIORITY}};

  for (const auto& priority_pair : priority_map) {
    // Create an Assistant notification.
    AddOrUpdateNotification(AssistantNotificationBuilder()
                                .WithId(kId)
                                .WithPriority(priority_pair.first)
                                .Build());

    // Verify expected system notification.
    auto* system_notification =
        message_center::MessageCenter::Get()->FindVisibleNotificationById(kId);
    ASSERT_NE(nullptr, system_notification);
    EXPECT_EQ(priority_pair.second, system_notification->priority());
  }
}

TEST_F(AssistantNotificationControllerTest, ShouldPropagateIsPinned) {
  constexpr char kId[] = "id";

  // Create an Assistant notification w/ default pin behavior.
  AddOrUpdateNotification(AssistantNotificationBuilder().WithId(kId).Build());

  // Verify expected system notification.
  auto* system_notification =
      message_center::MessageCenter::Get()->FindVisibleNotificationById(kId);
  ASSERT_NE(nullptr, system_notification);
  EXPECT_FALSE(system_notification->pinned());

  // Create an Assistant notification w/ explicitly disabled pin behavior.
  AddOrUpdateNotification(
      AssistantNotificationBuilder().WithId(kId).WithIsPinned(false).Build());

  // Verify expected system notification.
  system_notification =
      message_center::MessageCenter::Get()->FindVisibleNotificationById(kId);
  ASSERT_NE(nullptr, system_notification);
  EXPECT_FALSE(system_notification->pinned());

  // Create an Assistant notification w/ explicitly enabled pin behavior.
  AddOrUpdateNotification(
      AssistantNotificationBuilder().WithId(kId).WithIsPinned(true).Build());

  // Verify expected system notification.
  system_notification =
      message_center::MessageCenter::Get()->FindVisibleNotificationById(kId);
  ASSERT_NE(nullptr, system_notification);
  EXPECT_TRUE(system_notification->pinned());
}

TEST_F(AssistantNotificationControllerTest, ShouldPropagateRenotify) {
  constexpr char kId[] = "id";

  // Create an Assistant notification w/ default renotify behavior.
  AddOrUpdateNotification(AssistantNotificationBuilder().WithId(kId).Build());

  // Verify expected system notification.
  auto* system_notification =
      message_center::MessageCenter::Get()->FindVisibleNotificationById(kId);
  ASSERT_NE(nullptr, system_notification);
  EXPECT_FALSE(system_notification->renotify());

  // Create an Assistant notification w/ explicitly disabled renotify behavior.
  AddOrUpdateNotification(
      AssistantNotificationBuilder().WithId(kId).WithRenotify(false).Build());

  // Verify expected system notification.
  system_notification =
      message_center::MessageCenter::Get()->FindVisibleNotificationById(kId);
  ASSERT_NE(nullptr, system_notification);
  EXPECT_FALSE(system_notification->renotify());

  // Create an Assistant notification w/ explicitly enabled renotify behavior.
  AddOrUpdateNotification(
      AssistantNotificationBuilder().WithId(kId).WithRenotify(true).Build());

  // Verify expected system notification.
  system_notification =
      message_center::MessageCenter::Get()->FindVisibleNotificationById(kId);
  ASSERT_NE(nullptr, system_notification);
  EXPECT_TRUE(system_notification->renotify());
}

TEST_F(AssistantNotificationControllerTest,
       ShouldMaybeRetrieveNotificationPayload) {
  constexpr char kId[] = "id";

  // Mock the Assistant service.
  StrictMock<AssistantServiceMock> service;
  controller().SetAssistant(&service);

  // Create an Assistant notification w/ default |from_server|.
  AddOrUpdateNotification(AssistantNotificationBuilder().WithId(kId).Build());

  // Click notification and verify we do *not* attempt to retrieve payload.
  EXPECT_CALL(service, RetrieveNotification).Times(0);
  message_center::MessageCenter::Get()->ClickOnNotification(kId);
  Mock::VerifyAndClearExpectations(&service);

  // Create an Assistant notification explicitly *not* |from_server|.
  AddOrUpdateNotification(
      AssistantNotificationBuilder().WithId(kId).WithFromServer(false).Build());

  // Click notification and verify we do *not* attempt to retrieve payload.
  EXPECT_CALL(service, RetrieveNotification).Times(0);
  message_center::MessageCenter::Get()->ClickOnNotification(kId);
  Mock::VerifyAndClearExpectations(&service);

  // Create an Assistant notification explicitly |from_server|.
  AddOrUpdateNotification(
      AssistantNotificationBuilder().WithId(kId).WithFromServer(true).Build());

  // Click notification and verify we *do* attempt to retrieve payload.
  EXPECT_CALL(service, RetrieveNotification);
  message_center::MessageCenter::Get()->ClickOnNotification(kId);
}

}  // namespace ash
