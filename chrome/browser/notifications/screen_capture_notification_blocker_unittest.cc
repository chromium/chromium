// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/screen_capture_notification_blocker.h"

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/test/metrics/histogram_tester.h"
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

message_center::Notification CreateNotification(const GURL& origin,
                                                const std::string& id) {
  return message_center::Notification(
      message_center::NOTIFICATION_TYPE_SIMPLE, id,
      /*title=*/std::u16string(),
      /*message=*/std::u16string(), /*icon=*/ui::ImageModel(),
      /*display_source=*/std::u16string(), origin, message_center::NotifierId(),
      message_center::RichNotificationData(), /*delegate=*/nullptr);
}

message_center::Notification CreateNotification(const GURL& origin) {
  return CreateNotification(origin, /*id=*/"id");
}

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

class ScreenCaptureNotificationBlockerTest
    : public testing::TestWithParam<bool> {
 public:
  ScreenCaptureNotificationBlockerTest() {
    if (GetParam()) {
      feature_list_.InitAndEnableFeature(
          features::kMuteNotificationSnoozeAction);
    } else {
      feature_list_.InitAndDisableFeature(
          features::kMuteNotificationSnoozeAction);
    }

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

  void SimulateClose(bool by_user) {
    std::optional<message_center::Notification> notification =
        GetMutedNotification();
    ASSERT_TRUE(notification);
    notification_service_->RemoveNotification(
        NotificationHandler::Type::NOTIFICATIONS_MUTED, notification->id(),
        by_user, /*silent=*/false);
  }

  void SimulateClick(const std::optional<int>& action_index) {
    std::optional<message_center::Notification> notification =
        GetMutedNotification();
    ASSERT_TRUE(notification);
    notification_service_->SimulateClick(
        NotificationHandler::Type::NOTIFICATIONS_MUTED, notification->id(),
        action_index,
        /*reply=*/std::nullopt);
  }

  std::optional<message_center::Notification> GetMutedNotification() {
    std::vector<message_center::Notification> notifications =
        notification_service_->GetDisplayedNotificationsForType(
            NotificationHandler::Type::NOTIFICATIONS_MUTED);
    // Only one instance of the notification should be on screen.
    EXPECT_LE(notifications.size(), 1u);

    if (notifications.empty())
      return std::nullopt;
    return notifications[0];
  }

 protected:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

 private:
  base::test::ScopedFeatureList feature_list_;
  TestingProfile profile_;
  content::TestWebContentsFactory web_contents_factory_;
  std::unique_ptr<StubNotificationDisplayService> notification_service_;
  raw_ptr<ScreenCaptureNotificationBlocker> blocker_;
};

TEST_P(ScreenCaptureNotificationBlockerTest, ShouldNotBlockWhenNotCapturing) {
  EXPECT_FALSE(blocker().ShouldBlockNotification(
      CreateNotification(GURL("https://example.com"))));
}

TEST_P(ScreenCaptureNotificationBlockerTest, ShouldNotBlockCapturingOrigin) {
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

TEST_P(ScreenCaptureNotificationBlockerTest, ShouldBlockWhenCapturing) {
  blocker().OnIsCapturingDisplayChanged(
      CreateWebContents(GURL("https://example1.com")), true);
  EXPECT_TRUE(blocker().ShouldBlockNotification(
      CreateNotification(GURL("https://example2.com"))));
}

TEST_P(ScreenCaptureNotificationBlockerTest, ShouldBlockWhenCapturingMutliple) {
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

TEST_P(ScreenCaptureNotificationBlockerTest, CapturingTwice) {
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

TEST_P(ScreenCaptureNotificationBlockerTest, StopUnknownContents) {
  content::WebContents* contents =
      CreateWebContents(GURL("https://example1.com"));
  blocker().OnIsCapturingDisplayChanged(contents, false);
  EXPECT_FALSE(blocker().ShouldBlockNotification(
      CreateNotification(GURL("https://example2.com"))));
}

TEST_P(ScreenCaptureNotificationBlockerTest,
       ObservesMediaStreamCaptureIndicator) {
  MediaStreamCaptureIndicator* indicator =
      MediaCaptureDevicesDispatcher::GetInstance()
          ->GetMediaStreamCaptureIndicator()
          .get();
  EXPECT_TRUE(blocker().observation_.IsObservingSource(indicator));
}

TEST_P(ScreenCaptureNotificationBlockerTest, ShowsMutedNotification) {
  EXPECT_FALSE(GetMutedNotification());

  blocker().OnIsCapturingDisplayChanged(
      CreateWebContents(GURL("https://example1.com")), true);
  blocker().OnBlockedNotification(
      CreateNotification(GURL("https://example2.com")), /*replaced*/ false);

  std::optional<message_center::Notification> notification =
      GetMutedNotification();
  ASSERT_TRUE(notification);

  EXPECT_TRUE(notification->renotify());
  EXPECT_EQ(message_center::NOTIFICATION_TYPE_SIMPLE, notification->type());
  EXPECT_EQ(l10n_util::GetPluralStringFUTF16(IDS_NOTIFICATION_MUTED_TITLE,
                                             /*count=*/1),
            notification->title());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_NOTIFICATION_MUTED_MESSAGE),
            notification->message());
  if (GetParam()) {
    ASSERT_EQ(2u, notification->buttons().size());
    EXPECT_EQ(l10n_util::GetStringUTF16(IDS_NOTIFICATION_MUTED_ACTION_SNOOZE),
              notification->buttons()[0].title);
    EXPECT_EQ(
        l10n_util::GetPluralStringFUTF16(IDS_NOTIFICATION_MUTED_ACTION_SHOW,
                                         /*count=*/1),
        notification->buttons()[1].title);
  } else {
    ASSERT_EQ(1u, notification->buttons().size());
    EXPECT_EQ(
        l10n_util::GetPluralStringFUTF16(IDS_NOTIFICATION_MUTED_ACTION_SHOW,
                                         /*count=*/1),
        notification->buttons()[0].title);
  }
}

TEST_P(ScreenCaptureNotificationBlockerTest, UpdatesMutedNotification) {
  constexpr int kCount = 10;
  blocker().OnIsCapturingDisplayChanged(
      CreateWebContents(GURL("https://example1.com")), true);

  for (int i = 0; i < kCount; ++i) {
    blocker().OnBlockedNotification(
        CreateNotification(GURL("https://example2.com")), /*replaced*/ false);
  }

  std::optional<message_center::Notification> notification =
      GetMutedNotification();
  ASSERT_TRUE(notification);

  EXPECT_EQ(
      l10n_util::GetPluralStringFUTF16(IDS_NOTIFICATION_MUTED_TITLE, kCount),
      notification->title());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_NOTIFICATION_MUTED_MESSAGE),
            notification->message());
}

TEST_P(ScreenCaptureNotificationBlockerTest, ClosesMutedNotification) {
  content::WebContents* contents =
      CreateWebContents(GURL("https://example1.com"));
  // No notification initially.
  blocker().OnIsCapturingDisplayChanged(contents, true);
  EXPECT_FALSE(GetMutedNotification());

  // Expect a notification once we block one.
  blocker().OnBlockedNotification(
      CreateNotification(GURL("https://example2.com")), /*replaced*/ false);
  EXPECT_TRUE(GetMutedNotification());

  // Expect notification to be closed when capturing stops.
  blocker().OnIsCapturingDisplayChanged(contents, false);
  EXPECT_FALSE(GetMutedNotification());
}

TEST_P(ScreenCaptureNotificationBlockerTest,
       ClosesMutedNotificationOnBodyClick) {
  blocker().OnIsCapturingDisplayChanged(
      CreateWebContents(GURL("https://example1.com")), true);
  blocker().OnBlockedNotification(
      CreateNotification(GURL("https://example2.com")), /*replaced*/ false);

  // Expect notification to be closed after clicking on its body.
  SimulateClick(/*action_index=*/std::nullopt);
  EXPECT_FALSE(GetMutedNotification());
}

TEST_P(ScreenCaptureNotificationBlockerTest, ShowsMutedNotificationAfterClose) {
  blocker().OnIsCapturingDisplayChanged(
      CreateWebContents(GURL("https://example1.com")), true);
  blocker().OnBlockedNotification(
      CreateNotification(GURL("https://example2.com")), /*replaced*/ false);

  // Blocking another notification after closing the muted one should show a new
  // one with an updated message.
  SimulateClick(/*action_index=*/std::nullopt);
  blocker().OnBlockedNotification(
      CreateNotification(GURL("https://example2.com")), /*replaced*/ false);

  std::optional<message_center::Notification> notification =
      GetMutedNotification();
  ASSERT_TRUE(notification);
  EXPECT_EQ(l10n_util::GetPluralStringFUTF16(IDS_NOTIFICATION_MUTED_TITLE,
                                             /*count=*/2),
            notification->title());
}

TEST_P(ScreenCaptureNotificationBlockerTest, SnoozeAction) {
  if (!GetParam())
    return;

  blocker().OnIsCapturingDisplayChanged(
      CreateWebContents(GURL("https://example1.com")), true);
  blocker().OnBlockedNotification(
      CreateNotification(GURL("https://example2.com")), /*replaced*/ false);

  // "Snooze" should close and prevent any future notification.
  SimulateClick(0);
  EXPECT_FALSE(GetMutedNotification());

  blocker().OnBlockedNotification(
      CreateNotification(GURL("https://example2.com")), /*replaced*/ false);
  EXPECT_FALSE(GetMutedNotification());
}

TEST_P(ScreenCaptureNotificationBlockerTest, SnoozeActionShowOnNextSession) {
  if (!GetParam())
    return;

  content::WebContents* contents =
      CreateWebContents(GURL("https://example1.com"));
  blocker().OnIsCapturingDisplayChanged(contents, true);
  blocker().OnBlockedNotification(
      CreateNotification(GURL("https://example2.com")), /*replaced*/ false);
  SimulateClick(0);
  EXPECT_FALSE(GetMutedNotification());

  // After hiding all notifications and stopping and starting capture we should
  // see notifications again.
  blocker().OnIsCapturingDisplayChanged(contents, false);
  blocker().OnIsCapturingDisplayChanged(contents, true);

  blocker().OnBlockedNotification(
      CreateNotification(GURL("https://example2.com")), /*replaced*/ false);
  EXPECT_TRUE(GetMutedNotification());
}

TEST_P(ScreenCaptureNotificationBlockerTest, ShowAction) {
  MockNotificationBlockerObserver observer;
  base::ScopedObservation<NotificationBlocker, NotificationBlocker::Observer>
      scoped_observer(&observer);
  scoped_observer.Observe(&blocker());

  EXPECT_CALL(observer, OnBlockingStateChanged);
  blocker().OnIsCapturingDisplayChanged(
      CreateWebContents(GURL("https://example1.com")), true);
  testing::Mock::VerifyAndClearExpectations(&observer);

  message_center::Notification notification =
      CreateNotification(GURL("https://example2.com"));
  blocker().OnBlockedNotification(notification, /*replaced*/ false);
  EXPECT_TRUE(blocker().ShouldBlockNotification(notification));

  // Showing should close the "Notifications muted" notification and allow
  // showing future web notifications.
  EXPECT_CALL(observer, OnBlockingStateChanged);
  SimulateClick(GetParam() ? 1 : 0);
  testing::Mock::VerifyAndClearExpectations(&observer);

  EXPECT_FALSE(GetMutedNotification());
  EXPECT_FALSE(blocker().ShouldBlockNotification(notification));
}

TEST_P(ScreenCaptureNotificationBlockerTest, SnoozeClickHistogram) {
  if (!GetParam())
    return;

  base::HistogramTester histogram_tester;

  content::WebContents* contents =
      CreateWebContents(GURL("https://example1.com"));
  blocker().OnIsCapturingDisplayChanged(contents, true);
  message_center::Notification notification =
      CreateNotification(GURL("https://example2.com"));

  blocker().OnBlockedNotification(notification, /*replaced*/ false);

  auto action_delay = base::Seconds(5);
  task_environment_.FastForwardBy(action_delay);
  SimulateClick(0);

  // Test showing another notification while snoozing.
  blocker().OnBlockedNotification(notification, /*replaced*/ false);
  blocker().OnIsCapturingDisplayChanged(contents, false);

  histogram_tester.ExpectUniqueSample(
      "Notifications.Blocker.ScreenCapture.SnoozedCount", /*sample=*/1,
      /*count=*/1);
}

TEST_P(ScreenCaptureNotificationBlockerTest, SessionEndHistograms) {
  base::HistogramTester histogram_tester;

  content::WebContents* contents =
      CreateWebContents(GURL("https://example1.com"));
  blocker().OnIsCapturingDisplayChanged(contents, true);

  message_center::Notification notification1 =
      CreateNotification(GURL("https://example2.com"), "id1");
  message_center::Notification notification2 =
      CreateNotification(GURL("https://example2.com"), "id2");
  message_center::Notification notification3 =
      CreateNotification(GURL("https://example2.com"), "id3");

  // Mute 3 unique notifications.
  blocker().OnBlockedNotification(notification1, /*replaced*/ false);
  blocker().OnBlockedNotification(notification2, /*replaced*/ false);
  blocker().OnBlockedNotification(notification3, /*replaced*/ false);

  // Replace 2 of the muted notifications.
  blocker().OnBlockedNotification(notification1, /*replaced*/ true);
  blocker().OnBlockedNotification(notification2, /*replaced*/ true);

  // Close 1 of the muted notifications.
  blocker().OnClosedNotification(notification1);

  blocker().OnIsCapturingDisplayChanged(contents, false);
  histogram_tester.ExpectUniqueSample(
      "Notifications.Blocker.ScreenCapture.MutedCount", /*sample=*/3,
      /*count=*/1);
  histogram_tester.ExpectUniqueSample(
      "Notifications.Blocker.ScreenCapture.ReplacedCount", /*sample=*/2,
      /*count=*/1);
  histogram_tester.ExpectUniqueSample(
      "Notifications.Blocker.ScreenCapture.ClosedCount", /*sample=*/1,
      /*count=*/1);
  if (GetParam()) {
    histogram_tester.ExpectUniqueSample(
        "Notifications.Blocker.ScreenCapture.SnoozedCount", /*sample=*/0,
        /*count=*/1);
  }
}

TEST_P(ScreenCaptureNotificationBlockerTest, SessionTimingHistograms) {
  base::HistogramTester histogram_tester;

  content::WebContents* contents =
      CreateWebContents(GURL("https://example1.com"));
  blocker().OnIsCapturingDisplayChanged(contents, true);

  message_center::Notification notification =
      CreateNotification(GURL("https://example2.com"));
  blocker().OnBlockedNotification(notification, /*replaced*/ false);

  auto click_delay = base::Seconds(3);
  task_environment_.FastForwardBy(click_delay);

  SimulateClick(GetParam() ? 1 : 0);

  auto session_delay = base::Seconds(5);
  task_environment_.FastForwardBy(session_delay);

  blocker().OnIsCapturingDisplayChanged(contents, false);

  histogram_tester.ExpectUniqueTimeSample(
      "Notifications.Blocker.ScreenCapture.RevealDuration", click_delay,
      /*count=*/1);
  histogram_tester.ExpectUniqueTimeSample(
      "Notifications.Blocker.ScreenCapture.SessionDuration",
      click_delay + session_delay,
      /*count=*/1);
}

INSTANTIATE_TEST_SUITE_P(,
                         ScreenCaptureNotificationBlockerTest,
                         testing::Bool());
