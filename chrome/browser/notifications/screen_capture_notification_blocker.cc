// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/screen_capture_notification_blocker.h"

#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"

ScreenCaptureNotificationBlocker::ScreenCaptureNotificationBlocker() {
  observer_.Add(MediaCaptureDevicesDispatcher::GetInstance()
                    ->GetMediaStreamCaptureIndicator()
                    .get());
}

ScreenCaptureNotificationBlocker::~ScreenCaptureNotificationBlocker() = default;

bool ScreenCaptureNotificationBlocker::ShouldBlockNotifications() {
  return !capturing_web_contents_.empty();
}

void ScreenCaptureNotificationBlocker::OnIsCapturingDisplayChanged(
    content::WebContents* web_contents,
    bool is_capturing_display) {
  if (is_capturing_display)
    capturing_web_contents_.insert(web_contents);
  else
    capturing_web_contents_.erase(web_contents);

  NotifyBlockingStateChanged();
}
