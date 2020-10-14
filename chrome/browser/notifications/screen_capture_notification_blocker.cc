// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/screen_capture_notification_blocker.h"

#include <algorithm>

#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "content/public/browser/web_contents.h"
#include "ui/message_center/public/cpp/notification.h"
#include "url/gurl.h"
#include "url/origin.h"

ScreenCaptureNotificationBlocker::ScreenCaptureNotificationBlocker() {
  observer_.Add(MediaCaptureDevicesDispatcher::GetInstance()
                    ->GetMediaStreamCaptureIndicator()
                    .get());
}

ScreenCaptureNotificationBlocker::~ScreenCaptureNotificationBlocker() = default;

bool ScreenCaptureNotificationBlocker::ShouldBlockNotification(
    const message_center::Notification& notification) {
  // Don't block if no WebContents currently captures the screen.
  if (capturing_web_contents_.empty())
    return false;

  // Otherwise block all notifications that belong to non-capturing origins.
  return std::none_of(
      capturing_web_contents_.begin(), capturing_web_contents_.end(),
      [&notification](content::WebContents* web_contents) {
        return url::IsSameOriginWith(notification.origin_url(),
                                     web_contents->GetLastCommittedURL());
      });
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
