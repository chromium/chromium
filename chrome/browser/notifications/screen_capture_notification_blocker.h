// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCREEN_CAPTURE_NOTIFICATION_BLOCKER_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCREEN_CAPTURE_NOTIFICATION_BLOCKER_H_

#include "base/containers/flat_set.h"
#include "base/gtest_prod_util.h"
#include "base/scoped_observer.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"
#include "chrome/browser/notifications/notification_blocker.h"

namespace content {
class WebContents;
}  // namespace content

// This notification blocker listens to the events when the user starts
// capturing a display. It will block notifications while such a capture is
// ongoing. Note that this does not include casting the whole display and only
// covers capturing via WebContents.
// TODO(crbug.com/1131375): Also block notifications while casting a screen.
class ScreenCaptureNotificationBlocker
    : public NotificationBlocker,
      public MediaStreamCaptureIndicator::Observer {
 public:
  ScreenCaptureNotificationBlocker();
  ScreenCaptureNotificationBlocker(const ScreenCaptureNotificationBlocker&) =
      delete;
  ScreenCaptureNotificationBlocker& operator=(
      const ScreenCaptureNotificationBlocker&) = delete;
  ~ScreenCaptureNotificationBlocker() override;

  // NotificationBlocker:
  bool ShouldBlockNotification(
      const message_center::Notification& notification) override;

  // MediaStreamCaptureIndicator::Observer:
  void OnIsCapturingDisplayChanged(content::WebContents* web_contents,
                                   bool is_capturing_display) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(ScreenCaptureNotificationBlockerTest,
                           ObservesMediaStreamCaptureIndicator);

  ScopedObserver<MediaStreamCaptureIndicator,
                 MediaStreamCaptureIndicator::Observer>
      observer_{this};

  // Storing raw pointers here is fine because MediaStreamCaptureIndicator
  // notifies us before the WebContents is destroyed.
  base::flat_set<content::WebContents*> capturing_web_contents_;
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCREEN_CAPTURE_NOTIFICATION_BLOCKER_H_
