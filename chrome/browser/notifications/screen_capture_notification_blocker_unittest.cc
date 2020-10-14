// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/screen_capture_notification_blocker.h"

#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_web_contents_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
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

class ScreenCaptureNotificationBlockerTest : public testing::Test {
 public:
  ScreenCaptureNotificationBlockerTest() = default;
  ~ScreenCaptureNotificationBlockerTest() override = default;

  ScreenCaptureNotificationBlocker& blocker() { return blocker_; }

  content::WebContents* CreateWebContents(const GURL& url) {
    content::WebContents* contents =
        web_contents_factory_.CreateWebContents(&profile_);
    content::NavigationSimulator::NavigateAndCommitFromBrowser(contents, url);
    return contents;
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  content::TestWebContentsFactory web_contents_factory_;
  ScreenCaptureNotificationBlocker blocker_;
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
