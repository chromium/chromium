// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/screen_capture_notification_blocker.h"

#include "base/optional.h"
#include "base/scoped_observer.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"
#include "chrome/browser/notifications/muted_notification_handler.h"
#include "chrome/browser/notifications/notification_blocker.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "chrome/browser/notifications/stub_notification_display_service.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_web_contents_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/public/cpp/notification.h"
#include "url/gurl.h"

namespace {

message_center::Notification CreateNotification(const GURL& origin) {
  return message_center::Notification(
      message_center::NOTIFICATION_TYPE_SIMPLE, /*id=*/"id",
      /*title=*/base::string16(),
      /*message=*/base::string16(), /*icon=*/gfx::Image(),
      /*display_source=*/base::string16(), origin, message_center::NotifierId(),
      message_center::RichNotificationData(), /*delegate=*/nullptr);
}

}  // namespace

namespace {
constexpr int kShowActionIndex = 0;
}  // namespace

class MockNotificationBlockerObserver : public NotificationBlocker::Observer {
 public:
  MockNotificationBlockerObserver() = default;
  MockNotificationBlockerObserver(const MockNotificationBlockerObserver&) =
      delete;
  MockNotificationBlockerObserver& operator=(
      const MockNotificationBlockerObserver&) = delete;
  ~MockNotificationBlockerObserver() override = default;

  // NotificationBlocker::Observer:
  MOCK_METHOD(void, OnBlockingStateChanged, (), (override));
};

class ScreenCaptureNotificationBlockerTest : public testing::Test {
 public:
  ScreenCaptureNotificationBlockerTest() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kMuteNotificationsDuringScreenShare);

    notification_service_ =
        std::make_unique<StubNotificationDisplayService>(&profile_);
    auto blocker = std::make_unique<ScreenCaptureNotificationBlocker>(
        notification_service_.get());
    blocker_ = blocker.get();

    notification_service_->OverrideNotificationHandlerForTesting(
        NotificationHandler::Type::NOTIFICATIONS_MUTED,
        std::make_unique<MutedNotificationHandler>(blocker_));

    NotificationDisplayQueue::NotificationBlockers blockers;
    blockers.push_back(std::move(blocker));
    notification_service_->SetBlockersForTesting(std::move(blockers));
  }

  ~ScreenCaptureNotificationBlockerTest() override = default;

  ScreenCaptureNotificationBlocker& blocker() { return *blocker_; }

  content::WebContents* CreateWebContents(const GURL& url) {
    content::WebContents* contents =
        web_contents_factory_.CreateWebContents(&profile_);
    content::NavigationSimulator::NavigateAndCommitFromBrowser(contents, url);
    return contents;
  }

  void SimulateClick(const base::Optional<int>& action_index) {
    base::Optional<message_center::Notification> notification =
        GetMutedNotification();
    ASSERT_TRUE(notification);
    notification_service_->SimulateClick(
        NotificationHandler::Type::NOTIFICATIONS_MUTED, notification->id(),
        action_index,
        /*reply=*/base::nullopt);
  }

  base::Optional<message_center::Notification> GetMutedNotification() {
    std::vector<message_center::Notification> notifications =
        notification_service_->GetDisplayedNotificationsForType(
            NotificationHandler::Type::NOTIFICATIONS_MUTED);
    // Only one instance of the notification should be on screen.
    EXPECT_LE(notifications.size(), 1u);

    if (notifications.empty())
      return base::nullopt;
    return notifications[0];
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  TestingProfile profile_;
  content::TestWebContentsFactory web_contents_factory_;
  std::unique_ptr<StubNotificationDisplayService> notification_service_;
  ScreenCaptureNotificationBlocker* blocker_;
};

TEST_F(ScreenCaptureNotificationBlockerTest, ShouldNotBlockWhenNotCapturing) {
  EXPECT_FALSE(blocker().ShouldBlockNotification(
      CreateNotification(GURL("https://example.com"))));
}

TEST_F(ScreenCaptureNotificationBlockerTest, ShouldNotBlockCapturingOrigin) {
  GURL origin1("https://example1.com");
  GURL origin2("https://example2.com");
  GURL origin3("https://example3.com");

  // |origin1| and |origin2| are capturing, |origin3| is not.
  blocker().OnIsCapturingDisplayChanged(CreateWebContents(origin1), true);
  blocker().OnIsCapturingDisplayChanged(CreateWebContents(origin2), true);

  EXPECT_FALSE(blocker().ShouldBlockNotification(CreateNotification(origin1)));
  EXPECT_FALSE(blocker().ShouldBlockNotification(CreateNotification(origin2)));
  EXPECT_TRUE(blocker().ShouldBlockNotification(CreateNotification(origin3)));
}

TEST_F(ScreenCaptureNotificationBlockerTest, ShouldBlockWhenCapturing) {
  blocker().OnIsCapturingDisplayChanged(
      CreateWebContents(GURL("https://example1.com")), true);
  EXPECT_TRUE(blocker().ShouldBlockNotification(
      CreateNotification(GURL("https://example2.com"))));
}

TEST_F(ScreenCaptureNotificationBlockerTest, ShouldBlockWhenCapturingMutliple) {
  content::WebContents* contents_1 =
      CreateWebContents(GURL("https://example1.com"));
  content::WebContents* contents_2 =
      CreateWebContents(GURL("https://example2.com"));

  blocker().OnIsCapturingDisplayChanged(contents_1, true);
  blocker().OnIsCapturingDisplayChanged(contents_2, true);
  EXPECT_TRUE(blocker().ShouldBlockNotification(
      CreateNotification(GURL("https://example3.com"))));

  blocker().OnIsCapturingDisplayChanged(contents_1, false);
  EXPECT_TRUE(blocker().ShouldBlockNotification(
      CreateNotification(GURL("https://example3.com"))));

  blocker().OnIsCapturingDisplayChanged(contents_2, false);
  EXPECT_FALSE(blocker().ShouldBlockNotification(
      CreateNotification(GURL("https://example3.com"))));
}

TEST_F(ScreenCaptureNotificationBlockerTest, CapturingTwice) {
  content::WebContents* contents =
      CreateWebContents(GURL("https://example1.com"));

  // Calling changed twice with the same contents should ignore the second call.
  blocker().OnIsCapturingDisplayChanged(contents, true);
  blocker().OnIsCapturingDisplayChanged(contents, true);
  EXPECT_TRUE(blocker().ShouldBlockNotification(
      CreateNotification(GURL("https://example2.com"))));

  blocker().OnIsCapturingDisplayChanged(contents, false);
  EXPECT_FALSE(blocker().ShouldBlockNotification(
      CreateNotification(GURL("https://example2.com"))));
}

TEST_F(ScreenCaptureNotificationBlockerTest, StopUnknownContents) {
  content::WebContents* contents =
      CreateWebContents(GURL("https://example1.com"));
  blocker().OnIsCapturingDisplayChanged(contents, false);
  EXPECT_FALSE(blocker().ShouldBlockNotification(
      CreateNotification(GURL("https://example2.com"))));
}

TEST_F(ScreenCaptureNotificationBlockerTest,
       ObservesMediaStreamCaptureIndicator) {
  MediaStreamCaptureIndicator* indicator =
      MediaCaptureDevicesDispatcher::GetInstance()
          ->GetMediaStreamCaptureIndicator()
          .get();
  EXPECT_TRUE(blocker().observer_.IsObserving(indicator));
}

TEST_F(ScreenCaptureNotificationBlockerTest, ShowsMutedNotification) {
  EXPECT_FALSE(GetMutedNotification());

  blocker().OnIsCapturingDisplayChanged(
      CreateWebContents(GURL("https://example1.com")), true);
  blocker().OnBlockedNotification(
      CreateNotification(GURL("https://example2.com")));

  base::Optional<message_center::Notification> notification =
      GetMutedNotification();
  ASSERT_TRUE(notification);

  EXPECT_TRUE(notification->renotify());
  EXPECT_EQ(message_center::NOTIFICATION_TYPE_SIMPLE, notification->type());
  EXPECT_EQ(l10n_util::GetPluralStringFUTF16(IDS_NOTIFICATION_MUTED_TITLE,
                                             /*count=*/1),
            notification->title());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_NOTIFICATION_MUTED_MESSAGE),
            notification->message());
  ASSERT_EQ(1u, notification->buttons().size());
  EXPECT_EQ(l10n_util::GetPluralStringFUTF16(IDS_NOTIFICATION_MUTED_ACTION_SHOW,
                                             /*count=*/1),
            notification->buttons()[0].title);
}

TEST_F(ScreenCaptureNotificationBlockerTest, UpdatesMutedNotification) {
  constexpr int kCount = 10;
  blocker().OnIsCapturingDisplayChanged(
      CreateWebContents(GURL("https://example1.com")), true);

  for (int i = 0; i < kCount; ++i) {
    blocker().OnBlockedNotification(
        CreateNotification(GURL("https://example2.com")));
  }

  base::Optional<message_center::Notification> notification =
      GetMutedNotification();
  ASSERT_TRUE(notification);

  EXPECT_EQ(
      l10n_util::GetPluralStringFUTF16(IDS_NOTIFICATION_MUTED_TITLE, kCount),
      notification->title());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_NOTIFICATION_MUTED_MESSAGE),
            notification->message());
  ASSERT_EQ(1u, notification->buttons().size());
  EXPECT_EQ(l10n_util::GetPluralStringFUTF16(IDS_NOTIFICATION_MUTED_ACTION_SHOW,
                                             kCount),
            notification->buttons()[0].title);
}

TEST_F(ScreenCaptureNotificationBlockerTest, ClosesMutedNotification) {
  content::WebContents* contents =
      CreateWebContents(GURL("https://example1.com"));
  // No notification initially.
  blocker().OnIsCapturingDisplayChanged(contents, true);
  EXPECT_FALSE(GetMutedNotification());

  // Expect a notification once we block one.
  blocker().OnBlockedNotification(
      CreateNotification(GURL("https://example2.com")));
  EXPECT_TRUE(GetMutedNotification());

  // Expect notification to be closed when capturing stops.
  blocker().OnIsCapturingDisplayChanged(contents, false);
  EXPECT_FALSE(GetMutedNotification());
}

TEST_F(ScreenCaptureNotificationBlockerTest,
       ClosesMutedNotificationOnBodyClick) {
  blocker().OnIsCapturingDisplayChanged(
      CreateWebContents(GURL("https://example1.com")), true);
  blocker().OnBlockedNotification(
      CreateNotification(GURL("https://example2.com")));

  // Expect notification to be closed after clicking on its body.
  SimulateClick(/*action_index=*/base::nullopt);
  EXPECT_FALSE(GetMutedNotification());
}

TEST_F(ScreenCaptureNotificationBlockerTest, ShowsMutedNotificationAfterClose) {
  blocker().OnIsCapturingDisplayChanged(
      CreateWebContents(GURL("https://example1.com")), true);
  blocker().OnBlockedNotification(
      CreateNotification(GURL("https://example2.com")));

  // Blocking another notification after closing the muted one should show a new
  // one with an updated message.
  SimulateClick(/*action_index=*/base::nullopt);
  blocker().OnBlockedNotification(
      CreateNotification(GURL("https://example2.com")));

  base::Optional<message_center::Notification> notification =
      GetMutedNotification();
  ASSERT_TRUE(notification);
  EXPECT_EQ(l10n_util::GetPluralStringFUTF16(IDS_NOTIFICATION_MUTED_TITLE,
                                             /*count=*/2),
            notification->title());
}

TEST_F(ScreenCaptureNotificationBlockerTest, ShowAction) {
  MockNotificationBlockerObserver observer;
  ScopedObserver<NotificationBlocker, NotificationBlocker::Observer>
      scoped_observer(&observer);
  scoped_observer.Add(&blocker());

  EXPECT_CALL(observer, OnBlockingStateChanged);
  blocker().OnIsCapturingDisplayChanged(
      CreateWebContents(GURL("https://example1.com")), true);
  testing::Mock::VerifyAndClearExpectations(&observer);

  message_center::Notification notification =
      CreateNotification(GURL("https://example2.com"));
  blocker().OnBlockedNotification(notification);
  EXPECT_TRUE(blocker().ShouldBlockNotification(notification));

  // Showing should close the "Notifications muted" notification and allow
  // showing future web notifications.
  EXPECT_CALL(observer, OnBlockingStateChanged);
  SimulateClick(kShowActionIndex);
  testing::Mock::VerifyAndClearExpectations(&observer);

  EXPECT_FALSE(GetMutedNotification());
  EXPECT_FALSE(blocker().ShouldBlockNotification(notification));
}
