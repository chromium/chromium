// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/screen_capture_notification_blocker.h"

#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_contents_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

class ScreenCaptureNotificationBlockerTest : public testing::Test {
 public:
  ScreenCaptureNotificationBlockerTest() = default;
  ~ScreenCaptureNotificationBlockerTest() override = default;

  ScreenCaptureNotificationBlocker& blocker() { return blocker_; }

  content::WebContents* CreateWebContents() {
    return web_contents_factory_.CreateWebContents(&profile_);
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  content::TestWebContentsFactory web_contents_factory_;
  ScreenCaptureNotificationBlocker blocker_;
};

TEST_F(ScreenCaptureNotificationBlockerTest, ShouldNotBlockWhenNotCapturing) {
  EXPECT_FALSE(blocker().ShouldBlockNotifications());
}

TEST_F(ScreenCaptureNotificationBlockerTest, ShouldBlockWhenCapturing) {
  blocker().OnIsCapturingDisplayChanged(CreateWebContents(), true);
  EXPECT_TRUE(blocker().ShouldBlockNotifications());
}

TEST_F(ScreenCaptureNotificationBlockerTest, ShouldBlockWhenCapturingMutliple) {
  content::WebContents* contents_1 = CreateWebContents();
  content::WebContents* contents_2 = CreateWebContents();

  blocker().OnIsCapturingDisplayChanged(contents_1, true);
  blocker().OnIsCapturingDisplayChanged(contents_2, true);
  EXPECT_TRUE(blocker().ShouldBlockNotifications());

  blocker().OnIsCapturingDisplayChanged(contents_1, false);
  EXPECT_TRUE(blocker().ShouldBlockNotifications());

  blocker().OnIsCapturingDisplayChanged(contents_2, false);
  EXPECT_FALSE(blocker().ShouldBlockNotifications());
}

TEST_F(ScreenCaptureNotificationBlockerTest, CapturingTwice) {
  content::WebContents* contents = CreateWebContents();

  // Calling changed twice with the same contents should ignore the second call.
  blocker().OnIsCapturingDisplayChanged(contents, true);
  blocker().OnIsCapturingDisplayChanged(contents, true);
  EXPECT_TRUE(blocker().ShouldBlockNotifications());

  blocker().OnIsCapturingDisplayChanged(contents, false);
  EXPECT_FALSE(blocker().ShouldBlockNotifications());
}

TEST_F(ScreenCaptureNotificationBlockerTest, StopUnknownContents) {
  content::WebContents* contents = CreateWebContents();
  blocker().OnIsCapturingDisplayChanged(contents, false);
  EXPECT_FALSE(blocker().ShouldBlockNotifications());
}

TEST_F(ScreenCaptureNotificationBlockerTest,
       ObservesMediaStreamCaptureIndicator) {
  MediaStreamCaptureIndicator* indicator =
      MediaCaptureDevicesDispatcher::GetInstance()
          ->GetMediaStreamCaptureIndicator()
          .get();
  EXPECT_TRUE(blocker().observer_.IsObserving(indicator));
}
