// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_display_service_impl.h"

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/notifications/notification_blocker.h"
#include "chrome/browser/notifications/notification_display_queue.h"
#include "chrome/browser/notifications/notification_platform_bridge_delegator.h"
#include "chrome/common/notifications/notification_operation.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_contents_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/nearby_sharing/nearby_sharing_service_factory.h"
#endif

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/notifications/muted_notification_handler.h"
#include "chrome/browser/notifications/screen_capture_notification_blocker.h"
#endif

namespace {

class FakeNotificationBlocker : public NotificationBlocker {
 public:
  FakeNotificationBlocker() = default;
  ~FakeNotificationBlocker() override = default;

  // NotificationDisplayQueue::NotificationBlocker:
  bool ShouldBlockNotification(
      const message_center::Notification& notification) override {
    return should_block_;
  }

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
    notification_origins_[notification.id()] = notification.origin_url();
  }

  void Close(NotificationHandler::Type notification_type,
             const std::string& notification_id) override {
    notification_ids_.erase(notification_id);
    notification_origins_.erase(notification_id);
  }

  void GetDisplayed(GetDisplayedNotificationsCallback callback) const override {
    std::move(callback).Run(notification_ids_, /*supports_sync=*/true);
  }

  void GetDisplayedForOrigin(
      const GURL& origin,
      GetDisplayedNotificationsCallback callback) const override {
    std::set<std::string> result;
    for (const auto& id : notification_ids_) {
      auto origin_it = notification_origins_.find(id);
      if (url::IsSameOriginWith(origin_it->second, origin)) {
        result.insert(id);
      }
    }
    std::move(callback).Run(result, /*supports_sync=*/true);
  }

 private:
  std::set<std::string> notification_ids_;
  std::map<std::string, GURL> notification_origins_;
};

message_center::Notification CreateNotification(const std::string& id,
                                                const GURL& origin = {}) {
  return message_center::Notification(
      message_center::NOTIFICATION_TYPE_SIMPLE, id, /*title=*/std::u16string(),
      /*message=*/std::u16string(), /*icon=*/ui::ImageModel(),
      /*display_source=*/std::u16string(), origin, message_center::NotifierId(),
      message_center::RichNotificationData(), /*delegate=*/nullptr);
}

}  // namespace

class BaseNotificationDisplayServiceImplTest : public testing::Test {
 protected:
  BaseNotificationDisplayServiceImplTest() = default;
  ~BaseNotificationDisplayServiceImplTest() override = default;

  // testing::Test:
  void SetUp() override {
    service_ = std::make_unique<NotificationDisplayServiceImpl>(&profile_);

    auto notification_delegator =
        std::make_unique<TestNotificationPlatformBridgeDelegator>(&profile_);
    notification_delegator_ = notification_delegator.get();

    service_->SetNotificationPlatformBridgeDelegatorForTesting(
        std::move(notification_delegator));
  }

  Profile* profile() { return &profile_; }

  NotificationDisplayServiceImpl& service() { return *service_; }

 protected:
  std::set<std::string> GetDisplayedServiceSync() {
    base::test::TestFuture<std::set<std::string>, bool> displayed;
    service_->GetDisplayed(displayed.GetCallback());
    return displayed.Get<0>();
  }

  std::set<std::string> GetDisplayedForOriginServiceSync(const GURL& origin) {
    base::test::TestFuture<std::set<std::string>, bool> displayed;
    service_->GetDisplayedForOrigin(origin, displayed.GetCallback());
    return displayed.Get<0>();
  }

  std::set<std::string> GetDisplayedPlatformSync() {
    base::test::TestFuture<std::set<std::string>, bool> displayed;
    notification_delegator_->GetDisplayed(displayed.GetCallback());
    return displayed.Get<0>();
  }

  std::set<std::string> GetDisplayedForOriginPlatformSync(const GURL& origin) {
    base::test::TestFuture<std::set<std::string>, bool> displayed;
    notification_delegator_->GetDisplayedForOrigin(origin,
                                                   displayed.GetCallback());
    return displayed.Get<0>();
  }

  void DisplayNotification(const std::string id, const GURL& origin = {}) {
    service_->Display(NotificationHandler::Type::WEB_PERSISTENT,
                      CreateNotification(id, origin), /*metadata=*/nullptr);
  }

  void CloseNotification(const std::string id) {
    service_->Close(NotificationHandler::Type::WEB_PERSISTENT, id);
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  std::unique_ptr<NotificationDisplayServiceImpl> service_;
  raw_ptr<TestNotificationPlatformBridgeDelegator> notification_delegator_ =
      nullptr;
};

// Test class that uses a FakeNotificationBlocker instead of the real ones.
class NotificationDisplayServiceImplTest
    : public BaseNotificationDisplayServiceImplTest {
 protected:
  NotificationDisplayServiceImplTest() = default;
  ~NotificationDisplayServiceImplTest() override = default;

  // BaseNotificationDisplayServiceImplTest:
  void SetUp() override {
    BaseNotificationDisplayServiceImplTest::SetUp();

    auto blocker = std::make_unique<FakeNotificationBlocker>();
    notification_blocker_ = blocker.get();

    NotificationDisplayQueue::NotificationBlockers blockers;
    blockers.push_back(std::move(blocker));
    service().SetBlockersForTesting(std::move(blockers));
  }

  FakeNotificationBlocker& notification_blocker() {
    return *notification_blocker_;
  }

 private:
  raw_ptr<FakeNotificationBlocker, DanglingUntriaged> notification_blocker_ =
      nullptr;
};

TEST_F(NotificationDisplayServiceImplTest, DisplayWithoutBlockers) {
  service().SetBlockersForTesting({});
  std::string notification_id = "id";
  GURL origin("https://example.com/");
  DisplayNotification(notification_id, origin);

  std::set<std::string> displayed = GetDisplayedServiceSync();
  EXPECT_EQ(1u, displayed.size());
  EXPECT_EQ(1u, displayed.count(notification_id));
  EXPECT_EQ(displayed, GetDisplayedPlatformSync());
  EXPECT_EQ(displayed, GetDisplayedForOriginServiceSync(origin));
  EXPECT_EQ(displayed, GetDisplayedForOriginPlatformSync(origin));
  EXPECT_TRUE(
      GetDisplayedForOriginServiceSync(GURL("https://foo.bar")).empty());
  EXPECT_TRUE(
      GetDisplayedForOriginPlatformSync(GURL("https://foo.bar")).empty());
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
  GURL origin("https://example.com/");
  DisplayNotification(notification_id, origin);

  std::set<std::string> displayed = GetDisplayedServiceSync();
  EXPECT_EQ(1u, displayed.size());
  EXPECT_EQ(1u, displayed.count(notification_id));
  EXPECT_TRUE(GetDisplayedPlatformSync().empty());
  EXPECT_EQ(displayed, GetDisplayedForOriginServiceSync(origin));
  EXPECT_TRUE(GetDisplayedForOriginPlatformSync(origin).empty());
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

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(NotificationDisplayServiceImplTest, NearbyNotificationHandler) {
  // Add the Nearby Share handler if and only if Nearby Share is supported.
  {
    NearbySharingServiceFactory::
        SetIsNearbyShareSupportedForBrowserContextForTesting(false);
    NotificationDisplayServiceImpl service(profile());
    EXPECT_FALSE(service.GetNotificationHandler(
        NotificationHandler::Type::NEARBY_SHARE));
  }
  {
    NearbySharingServiceFactory::
        SetIsNearbyShareSupportedForBrowserContextForTesting(true);
    NotificationDisplayServiceImpl service(profile());
    EXPECT_TRUE(service.GetNotificationHandler(
        NotificationHandler::Type::NEARBY_SHARE));
  }
}
#endif

#if !BUILDFLAG(IS_ANDROID)

// Desktop specific test class that uses the default NotificationBlockers.
class DesktopNotificationDisplayServiceImplTest
    : public BaseNotificationDisplayServiceImplTest {
 protected:
  DesktopNotificationDisplayServiceImplTest() = default;
  ~DesktopNotificationDisplayServiceImplTest() override = default;

  // BaseNotificationDisplayServiceImplTest:
  void SetUp() override {
    BaseNotificationDisplayServiceImplTest::SetUp();

    auto* muted_handler =
        static_cast<MutedNotificationHandler*>(service().GetNotificationHandler(
            NotificationHandler::Type::NOTIFICATIONS_MUTED));
    ASSERT_TRUE(muted_handler);
    screen_capture_blocker_ = static_cast<ScreenCaptureNotificationBlocker*>(
        muted_handler->get_delegate_for_testing());
    ASSERT_TRUE(screen_capture_blocker_);
  }

  ScreenCaptureNotificationBlocker* screen_capture_notification_blocker() {
    return screen_capture_blocker_;
  }

 private:
  raw_ptr<ScreenCaptureNotificationBlocker> screen_capture_blocker_ = nullptr;
};

TEST_F(DesktopNotificationDisplayServiceImplTest, SnoozeDuringScreenCapture) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kMuteNotificationSnoozeAction);

  content::TestWebContentsFactory web_contents_factory;
  content::WebContents* contents =
      web_contents_factory.CreateWebContents(profile());
  EXPECT_TRUE(GetDisplayedPlatformSync().empty());

  // Start a screen capture session.
  screen_capture_notification_blocker()->OnIsCapturingDisplayChanged(
      contents, /*is_capturing_display=*/true);

  // Displaying a notification should show the "Notifications Muted" generic
  // notification instead of the real notification content.
  std::string notification_id_1 = "id1";
  DisplayNotification(notification_id_1);
  EXPECT_EQ(1u, GetDisplayedPlatformSync().size());
  EXPECT_EQ(1u, GetDisplayedPlatformSync().count(kMuteNotificationId));

  // Emulate the user clicking on the "Snooze" action button.
  service().ProcessNotificationOperation(
      NotificationOperation::kClick,
      NotificationHandler::Type::NOTIFICATIONS_MUTED, /*origin=*/GURL(),
      kMuteNotificationId, /*action_index=*/0, /*reply=*/std::nullopt,
      /*by_user=*/true);

  // Clicking "Snooze" should remove the "Notifications Muted" notification.
  EXPECT_TRUE(GetDisplayedPlatformSync().empty());

  // Showing another notification should not trigger any visible notification.
  std::string notification_id_2 = "id2";
  DisplayNotification(notification_id_2);
  EXPECT_TRUE(GetDisplayedPlatformSync().empty());

  // Stopping the screen sharing session should re-display all previously muted
  // notifications.
  screen_capture_notification_blocker()->OnIsCapturingDisplayChanged(
      contents, /*is_capturing_display=*/false);
  EXPECT_EQ(2u, GetDisplayedPlatformSync().size());
  EXPECT_EQ(1u, GetDisplayedPlatformSync().count(notification_id_1));
  EXPECT_EQ(1u, GetDisplayedPlatformSync().count(notification_id_2));
}

#endif  // !BUILDFLAG(IS_ANDROID)
