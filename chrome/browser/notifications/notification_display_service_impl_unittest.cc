// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/bind_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/string16.h"
#include "base/test/bind_test_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/notification_blocker.h"
#include "chrome/browser/notifications/notification_display_queue.h"
#include "chrome/browser/notifications/notification_display_service_impl.h"
#include "chrome/browser/notifications/notification_platform_bridge_delegator.h"
#include "chrome/browser/notifications/stub_notification_platform_bridge.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "url/gurl.h"

namespace {

class FakeNotificationBlocker : public NotificationBlocker {
 public:
  FakeNotificationBlocker() = default;
  ~FakeNotificationBlocker() override = default;

  // NotificationDisplayQueue::NotificationBlocker:
  bool ShouldBlockNotifications() override { return should_block_; }

  void SetShouldBlockNotifications(bool should_block) {
    should_block_ = should_block;
    NotifyBlockingStateChanged();
  }

 private:
  bool should_block_ = false;
};

class TestNotificationPlatformBridgeDelegator
    : public NotificationPlatformBridgeDelegator {
 public:
  explicit TestNotificationPlatformBridgeDelegator(Profile* profile)
      : NotificationPlatformBridgeDelegator(profile, base::DoNothing()) {}
  ~TestNotificationPlatformBridgeDelegator() override = default;

  // NotificationPlatformBridgeDelegator:
  void Display(
      NotificationHandler::Type notification_type,
      const message_center::Notification& notification,
      std::unique_ptr<NotificationCommon::Metadata> metadata) override {
    notification_ids_.insert(notification.id());
  }

  void Close(NotificationHandler::Type notification_type,
             const std::string& notification_id) override {
    notification_ids_.erase(notification_id);
  }

  void GetDisplayed(GetDisplayedNotificationsCallback callback) const override {
    std::move(callback).Run(notification_ids_, /*supports_sync=*/true);
  }

 private:
  std::set<std::string> notification_ids_;
};

message_center::Notification CreateNotification(const std::string& id) {
  return message_center::Notification(
      message_center::NOTIFICATION_TYPE_SIMPLE, id, /*title=*/base::string16(),
      /*message=*/base::string16(), /*icon=*/gfx::Image(),
      /*display_source=*/base::string16(),
      /*origin_url=*/GURL(), message_center::NotifierId(),
      message_center::RichNotificationData(),
      base::MakeRefCounted<message_center::NotificationDelegate>());
}

}  // namespace

class NotificationDisplayServiceImplTest : public testing::Test {
 protected:
  NotificationDisplayServiceImplTest() = default;
  ~NotificationDisplayServiceImplTest() override = default;

  // BrowserWithTestWindowTest:
  void SetUp() override {
    TestingBrowserProcess* browser_process = TestingBrowserProcess::GetGlobal();
    if (browser_process) {
      browser_process->SetNotificationPlatformBridge(
          std::make_unique<StubNotificationPlatformBridge>());
    }

    service_ = std::make_unique<NotificationDisplayServiceImpl>(&profile_);

    auto notification_delegator =
        std::make_unique<TestNotificationPlatformBridgeDelegator>(&profile_);
    notification_delegator_ = notification_delegator.get();

    service_->SetNotificationPlatformBridgeDelegatorForTesting(
        std::move(notification_delegator));

    auto blocker = std::make_unique<FakeNotificationBlocker>();
    notification_blocker_ = blocker.get();

    NotificationDisplayQueue::NotificationBlockers blockers;
    blockers.push_back(std::move(blocker));
    service_->SetBlockersForTesting(std::move(blockers));
  }

  NotificationDisplayServiceImpl& service() { return *service_; }

  FakeNotificationBlocker& notification_blocker() {
    return *notification_blocker_;
  }

 protected:
  std::set<std::string> GetDisplayedServiceSync() {
    std::set<std::string> displayed_ids;
    base::RunLoop run_loop;
    service_->GetDisplayed(
        base::BindLambdaForTesting([&](std::set<std::string> notification_ids,
                                       bool supports_synchronization) {
          displayed_ids = std::move(notification_ids);
          run_loop.Quit();
        }));
    run_loop.Run();
    return displayed_ids;
  }

  std::set<std::string> GetDisplayedPlatformSync() {
    std::set<std::string> displayed_ids;
    base::RunLoop run_loop;
    notification_delegator_->GetDisplayed(
        base::BindLambdaForTesting([&](std::set<std::string> notification_ids,
                                       bool supports_synchronization) {
          displayed_ids = std::move(notification_ids);
          run_loop.Quit();
        }));
    run_loop.Run();
    return displayed_ids;
  }

  void DisplayNotification(const std::string id) {
    service_->Display(NotificationHandler::Type::TRANSIENT,
                      CreateNotification(id), /*metadata=*/nullptr);
  }

  void CloseNotification(const std::string id) {
    service_->Close(NotificationHandler::Type::TRANSIENT, id);
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  std::unique_ptr<NotificationDisplayServiceImpl> service_;
  TestNotificationPlatformBridgeDelegator* notification_delegator_ = nullptr;
  FakeNotificationBlocker* notification_blocker_ = nullptr;
};

TEST_F(NotificationDisplayServiceImplTest, DisplayWithoutBlockers) {
  service().SetBlockersForTesting({});
  std::string notification_id = "id";
  DisplayNotification(notification_id);

  std::set<std::string> displayed = GetDisplayedServiceSync();
  EXPECT_EQ(1u, displayed.size());
  EXPECT_EQ(1u, displayed.count(notification_id));
  EXPECT_EQ(displayed, GetDisplayedPlatformSync());
}

TEST_F(NotificationDisplayServiceImplTest, DisplayWithAllowingBlocker) {
  std::string notification_id = "id";
  DisplayNotification(notification_id);

  std::set<std::string> displayed = GetDisplayedServiceSync();
  EXPECT_EQ(1u, displayed.size());
  EXPECT_EQ(1u, displayed.count(notification_id));
  EXPECT_EQ(displayed, GetDisplayedPlatformSync());
}

TEST_F(NotificationDisplayServiceImplTest, DisplayWithBlockingBlocker) {
  notification_blocker().SetShouldBlockNotifications(true);
  std::string notification_id = "id";
  DisplayNotification(notification_id);

  std::set<std::string> displayed = GetDisplayedServiceSync();
  EXPECT_EQ(1u, displayed.size());
  EXPECT_EQ(1u, displayed.count(notification_id));
  EXPECT_TRUE(GetDisplayedPlatformSync().empty());
}

TEST_F(NotificationDisplayServiceImplTest, UnblockQueuedNotification) {
  notification_blocker().SetShouldBlockNotifications(true);
  std::string notification_id = "id";
  DisplayNotification(notification_id);
  EXPECT_TRUE(GetDisplayedPlatformSync().empty());

  notification_blocker().SetShouldBlockNotifications(false);
  std::set<std::string> displayed = GetDisplayedServiceSync();
  EXPECT_EQ(1u, displayed.size());
  EXPECT_EQ(1u, displayed.count(notification_id));
  EXPECT_EQ(displayed, GetDisplayedPlatformSync());
}

TEST_F(NotificationDisplayServiceImplTest, CloseQueuedNotification) {
  notification_blocker().SetShouldBlockNotifications(true);
  std::string notification_id = "id";
  DisplayNotification(notification_id);
  EXPECT_EQ(1u, GetDisplayedServiceSync().size());
  EXPECT_TRUE(GetDisplayedPlatformSync().empty());

  CloseNotification(notification_id);
  EXPECT_TRUE(GetDisplayedServiceSync().empty());
  EXPECT_TRUE(GetDisplayedPlatformSync().empty());

  notification_blocker().SetShouldBlockNotifications(false);
  EXPECT_TRUE(GetDisplayedServiceSync().empty());
  EXPECT_TRUE(GetDisplayedPlatformSync().empty());
}
