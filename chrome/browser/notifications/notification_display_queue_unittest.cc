// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/browser/notifications/notification_blocker.h"
#include "chrome/browser/notifications/notification_display_queue.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

class FakeNotificationBlocker : public NotificationBlocker {
 public:
  FakeNotificationBlocker() = default;
  ~FakeNotificationBlocker() override = default;

  // NotificationDisplayQueue::NotificationBlocker:
  bool ShouldBlockNotification(
      const message_center::Notification& notification) override {
    if (!should_block_ || !blocked_origin_)
      return should_block_;

    return url::IsSameOriginWith(notification.origin_url(), *blocked_origin_);
  }
  MOCK_METHOD(void,
              OnBlockedNotification,
              (const message_center::Notification&, bool),
              (override));
  MOCK_METHOD(void,
              OnClosedNotification,
              (const message_center::Notification&),
              (override));

  void SetShouldBlockNotifications(bool should_block) {
    should_block_ = should_block;
    NotifyBlockingStateChanged();
  }

  void SetBlockedOrigin(const std::optional<GURL>& blocked_origin) {
    blocked_origin_ = blocked_origin;
    NotifyBlockingStateChanged();
  }

 private:
  bool should_block_ = false;
  std::optional<GURL> blocked_origin_;
};

class NotificationDisplayServiceMock : public NotificationDisplayService {
 public:
  NotificationDisplayServiceMock() = default;
  ~NotificationDisplayServiceMock() override = default;

  using NotificationDisplayService::DisplayedNotificationsCallback;

  MOCK_METHOD3(DisplayMockImpl,
               void(NotificationHandler::Type,
                    const message_center::Notification&,
                    NotificationCommon::Metadata*));
  void Display(
      NotificationHandler::Type notification_type,
      const message_center::Notification& notification,
      std::unique_ptr<NotificationCommon::Metadata> metadata) override {
    DisplayMockImpl(notification_type, notification, metadata.get());
  }

  MOCK_METHOD2(Close, void(NotificationHandler::Type, const std::string&));
  MOCK_METHOD1(GetDisplayed, void(DisplayedNotificationsCallback));
  MOCK_METHOD2(GetDisplayedForOrigin,
               void(const GURL& origin, DisplayedNotificationsCallback));
  MOCK_METHOD1(AddObserver, void(Observer* observer));
  MOCK_METHOD1(RemoveObserver, void(Observer* observer));
};

// Matcher to compare Notifications
MATCHER_P(EqualNotification, notification, "") {
  return arg.type() == notification.type() && arg.id() == notification.id();
}

message_center::Notification CreateNotification(const std::string& id,
                                                const GURL& origin) {
  return message_center::Notification(
      message_center::NOTIFICATION_TYPE_SIMPLE, id, /*title=*/std::u16string(),
      /*message=*/std::u16string(), /*icon=*/ui::ImageModel(),
      /*display_source=*/std::u16string(), origin, message_center::NotifierId(),
      message_center::RichNotificationData(),
      base::MakeRefCounted<message_center::NotificationDelegate>());
}

message_center::Notification CreateNotification(const std::string& id) {
  return CreateNotification(id, GURL());
}

}  // namespace

class NotificationDisplayQueueTest : public testing::Test {
 protected:
  NotificationDisplayQueueTest() = default;
  ~NotificationDisplayQueueTest() override = default;

  // testing::Test:
  void SetUp() override {
    auto blocker = std::make_unique<FakeNotificationBlocker>();
    notification_blocker_ = blocker.get();

    NotificationDisplayQueue::NotificationBlockers blockers;
    blockers.push_back(std::move(blocker));
    queue_.SetNotificationBlockers(std::move(blockers));
  }

  NotificationDisplayServiceMock& service() { return service_; }

  NotificationDisplayQueue& queue() { return queue_; }

  FakeNotificationBlocker& notification_blocker() {
    return *notification_blocker_;
  }

 private:
  NotificationDisplayServiceMock service_;
  NotificationDisplayQueue queue_{&service_};
  raw_ptr<FakeNotificationBlocker, DanglingUntriaged> notification_blocker_ =
      nullptr;
};

TEST_F(NotificationDisplayQueueTest, ShouldEnqueueWithoutBlockers) {
  queue().SetNotificationBlockers({});
  EXPECT_FALSE(queue().ShouldEnqueueNotification(
      NotificationHandler::Type::WEB_PERSISTENT, CreateNotification("id")));
}

TEST_F(NotificationDisplayQueueTest, ShouldEnqueueWithAllowingBlocker) {
  EXPECT_FALSE(queue().ShouldEnqueueNotification(
      NotificationHandler::Type::WEB_PERSISTENT, CreateNotification("id")));
}

TEST_F(NotificationDisplayQueueTest, ShouldEnqueueWithBlockingBlocker) {
  notification_blocker().SetShouldBlockNotifications(true);
  EXPECT_TRUE(queue().ShouldEnqueueNotification(
      NotificationHandler::Type::WEB_PERSISTENT, CreateNotification("id")));
}

TEST_F(NotificationDisplayQueueTest, ShouldEnqueueForNonWebNotification) {
  notification_blocker().SetShouldBlockNotifications(true);
  EXPECT_FALSE(queue().ShouldEnqueueNotification(
      NotificationHandler::Type::TRANSIENT, CreateNotification("id")));
}

TEST_F(NotificationDisplayQueueTest, EnqueueNotification) {
  std::string notification_id = "id";
  GURL origin("https://example.com");
  queue().EnqueueNotification(NotificationHandler::Type::TRANSIENT,
                              CreateNotification(notification_id, origin),
                              /*metadata=*/nullptr);
  EXPECT_THAT(queue().GetQueuedNotificationIds(),
              testing::ElementsAre(notification_id));
  EXPECT_THAT(queue().GetQueuedNotificationIdsForOrigin(origin),
              testing::ElementsAre(notification_id));
  EXPECT_TRUE(queue()
                  .GetQueuedNotificationIdsForOrigin(GURL("https://foo.bar"))
                  .empty());
}

TEST_F(NotificationDisplayQueueTest, RemoveQueuedNotification) {
  std::string notification_id_1 = "id1";
  std::string notification_id_2 = "id2";
  std::string notification_id_3 = "id3";

  queue().EnqueueNotification(NotificationHandler::Type::TRANSIENT,
                              CreateNotification(notification_id_1),
                              /*metadata=*/nullptr);
  queue().EnqueueNotification(NotificationHandler::Type::TRANSIENT,
                              CreateNotification(notification_id_2),
                              /*metadata=*/nullptr);
  queue().EnqueueNotification(NotificationHandler::Type::TRANSIENT,
                              CreateNotification(notification_id_3),
                              /*metadata=*/nullptr);
  EXPECT_EQ(3u, queue().GetQueuedNotificationIds().size());

  queue().RemoveQueuedNotification(notification_id_2);
  std::set<std::string> queued = queue().GetQueuedNotificationIds();
  EXPECT_EQ(2u, queued.size());
  EXPECT_EQ(1u, queued.count(notification_id_1));
  EXPECT_EQ(1u, queued.count(notification_id_3));
}

TEST_F(NotificationDisplayQueueTest, BlockUnblockBlocker) {
  EXPECT_CALL(service(), DisplayMockImpl).Times(0);
  notification_blocker().SetShouldBlockNotifications(true);
  notification_blocker().SetShouldBlockNotifications(false);
}

TEST_F(NotificationDisplayQueueTest, BlockUnblockMultipleBlockers) {
  auto blocker_1 = std::make_unique<FakeNotificationBlocker>();
  FakeNotificationBlocker* notification_blocker_1 = blocker_1.get();
  auto blocker_2 = std::make_unique<FakeNotificationBlocker>();
  FakeNotificationBlocker* notification_blocker_2 = blocker_2.get();

  NotificationDisplayQueue::NotificationBlockers blockers;
  blockers.push_back(std::move(blocker_1));
  blockers.push_back(std::move(blocker_2));
  queue().SetNotificationBlockers(std::move(blockers));

  EXPECT_CALL(service(), DisplayMockImpl).Times(0);
  notification_blocker_1->SetShouldBlockNotifications(true);
  notification_blocker_2->SetShouldBlockNotifications(true);

  EXPECT_CALL(*notification_blocker_1, OnBlockedNotification);
  EXPECT_CALL(*notification_blocker_2, OnBlockedNotification);

  message_center::Notification notification = CreateNotification("id");
  queue().EnqueueNotification(NotificationHandler::Type::TRANSIENT,
                              notification, /*metadata=*/nullptr);

  notification_blocker_2->SetShouldBlockNotifications(false);
  EXPECT_CALL(service(), DisplayMockImpl(NotificationHandler::Type::TRANSIENT,
                                         EqualNotification(notification),
                                         /*metadata=*/nullptr))
      .Times(1);
  notification_blocker_1->SetShouldBlockNotifications(false);
}

TEST_F(NotificationDisplayQueueTest, UnblockNotificationOrdering) {
  notification_blocker().SetShouldBlockNotifications(true);

  message_center::Notification notification_1 = CreateNotification("id1");
  message_center::Notification notification_2 = CreateNotification("id2");
  message_center::Notification notification_3 = CreateNotification("id3");

  EXPECT_CALL(notification_blocker(), OnBlockedNotification).Times(3);

  queue().EnqueueNotification(NotificationHandler::Type::TRANSIENT,
                              notification_1, /*metadata=*/nullptr);
  queue().EnqueueNotification(NotificationHandler::Type::TRANSIENT,
                              notification_2, /*metadata=*/nullptr);
  queue().EnqueueNotification(NotificationHandler::Type::TRANSIENT,
                              notification_3, /*metadata=*/nullptr);
  EXPECT_EQ(3u, queue().GetQueuedNotificationIds().size());

  testing::InSequence s;
  EXPECT_CALL(service(), DisplayMockImpl(NotificationHandler::Type::TRANSIENT,
                                         EqualNotification(notification_1),
                                         /*metadata=*/nullptr));
  EXPECT_CALL(service(), DisplayMockImpl(NotificationHandler::Type::TRANSIENT,
                                         EqualNotification(notification_2),
                                         /*metadata=*/nullptr));
  EXPECT_CALL(service(), DisplayMockImpl(NotificationHandler::Type::TRANSIENT,
                                         EqualNotification(notification_3),
                                         /*metadata=*/nullptr));
  notification_blocker().SetShouldBlockNotifications(false);
}

TEST_F(NotificationDisplayQueueTest, UnblockNotificationSubset) {
  GURL origin_1("https://example1.com");
  GURL origin_2("https://example2.com");

  auto blocker_1 = std::make_unique<FakeNotificationBlocker>();
  FakeNotificationBlocker* notification_blocker_1 = blocker_1.get();
  notification_blocker_1->SetShouldBlockNotifications(true);
  notification_blocker_1->SetBlockedOrigin(origin_1);

  auto blocker_2 = std::make_unique<FakeNotificationBlocker>();
  FakeNotificationBlocker* notification_blocker_2 = blocker_2.get();
  notification_blocker_2->SetShouldBlockNotifications(true);
  notification_blocker_2->SetBlockedOrigin(origin_2);

  NotificationDisplayQueue::NotificationBlockers blockers;
  blockers.push_back(std::move(blocker_1));
  blockers.push_back(std::move(blocker_2));
  queue().SetNotificationBlockers(std::move(blockers));

  message_center::Notification notification_1 =
      CreateNotification("id1", origin_1);
  message_center::Notification notification_2 =
      CreateNotification("id2", origin_2);

  queue().EnqueueNotification(NotificationHandler::Type::TRANSIENT,
                              notification_1, /*metadata=*/nullptr);
  queue().EnqueueNotification(NotificationHandler::Type::TRANSIENT,
                              notification_2, /*metadata=*/nullptr);
  EXPECT_EQ(2u, queue().GetQueuedNotificationIds().size());

  // Unblocking first blocker should only show first notification.
  EXPECT_CALL(service(), DisplayMockImpl(NotificationHandler::Type::TRANSIENT,
                                         EqualNotification(notification_1),
                                         /*metadata=*/nullptr));
  notification_blocker_1->SetShouldBlockNotifications(false);
  testing::Mock::VerifyAndClearExpectations(&service());
  EXPECT_EQ(1u, queue().GetQueuedNotificationIds().size());

  // Unblocking second blocker should show second notification.
  EXPECT_CALL(service(), DisplayMockImpl(NotificationHandler::Type::TRANSIENT,
                                         EqualNotification(notification_2),
                                         /*metadata=*/nullptr));
  notification_blocker_2->SetShouldBlockNotifications(false);
  testing::Mock::VerifyAndClearExpectations(&service());
  EXPECT_EQ(0u, queue().GetQueuedNotificationIds().size());
}

TEST_F(NotificationDisplayQueueTest, MultipleBlockersNotifyBlocked) {
  auto blocker_1 = std::make_unique<FakeNotificationBlocker>();
  FakeNotificationBlocker* notification_blocker_1 = blocker_1.get();
  auto blocker_2 = std::make_unique<FakeNotificationBlocker>();
  FakeNotificationBlocker* notification_blocker_2 = blocker_2.get();

  NotificationDisplayQueue::NotificationBlockers blockers;
  blockers.push_back(std::move(blocker_1));
  blockers.push_back(std::move(blocker_2));
  queue().SetNotificationBlockers(std::move(blockers));

  notification_blocker_1->SetShouldBlockNotifications(true);
  notification_blocker_2->SetShouldBlockNotifications(false);

  EXPECT_CALL(*notification_blocker_1, OnBlockedNotification).Times(1);
  EXPECT_CALL(*notification_blocker_2, OnBlockedNotification).Times(0);

  queue().EnqueueNotification(NotificationHandler::Type::TRANSIENT,
                              CreateNotification("id1"), /*metadata=*/nullptr);

  notification_blocker_2->SetShouldBlockNotifications(true);
  notification_blocker_1->SetShouldBlockNotifications(false);

  EXPECT_CALL(*notification_blocker_1, OnBlockedNotification).Times(0);
  EXPECT_CALL(*notification_blocker_2, OnBlockedNotification).Times(1);

  queue().EnqueueNotification(NotificationHandler::Type::TRANSIENT,
                              CreateNotification("id2"), /*metadata=*/nullptr);
}

TEST_F(NotificationDisplayQueueTest, NotifiesReplacedNotification) {
  message_center::Notification notification = CreateNotification("id");
  notification_blocker().SetShouldBlockNotifications(true);

  EXPECT_CALL(notification_blocker(),
              OnBlockedNotification(testing::_, /*replaced=*/false));
  queue().EnqueueNotification(NotificationHandler::Type::TRANSIENT,
                              notification, /*metadata=*/nullptr);

  EXPECT_CALL(notification_blocker(),
              OnBlockedNotification(testing::_, /*replaced=*/true));
  EXPECT_CALL(notification_blocker(), OnClosedNotification).Times(0);
  queue().EnqueueNotification(NotificationHandler::Type::TRANSIENT,
                              notification, /*metadata=*/nullptr);
}

TEST_F(NotificationDisplayQueueTest, NotifiesClosedNotification) {
  message_center::Notification notification = CreateNotification("id");
  notification_blocker().SetShouldBlockNotifications(true);

  EXPECT_CALL(notification_blocker(), OnBlockedNotification);
  queue().EnqueueNotification(NotificationHandler::Type::TRANSIENT,
                              notification, /*metadata=*/nullptr);

  EXPECT_CALL(notification_blocker(), OnClosedNotification);
  queue().RemoveQueuedNotification(notification.id());
}
