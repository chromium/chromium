// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCREEN_CAPTURE_NOTIFICATION_BLOCKER_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCREEN_CAPTURE_NOTIFICATION_BLOCKER_H_

#include "base/containers/flat_set.h"
#include "base/gtest_prod_util.h"
#include "base/scoped_observer.h"
#include "base/time/time.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"
#include "chrome/browser/notifications/muted_notification_handler.h"
#include "chrome/browser/notifications/notification_blocker.h"

namespace content {
class WebContents;
}  // namespace content

class NotificationDisplayService;

// This notification blocker listens to the events when the user starts
// capturing a display. It will block notifications while such a capture is
// ongoing. Note that this does not include casting the whole display and only
// covers capturing via WebContents.
// TODO(crbug.com/1131375): Also block notifications while casting a screen.
class ScreenCaptureNotificationBlocker
    : public NotificationBlocker,
      public MutedNotificationHandler::Delegate,
      public MediaStreamCaptureIndicator::Observer {
 public:
  explicit ScreenCaptureNotificationBlocker(
      NotificationDisplayService* notification_display_service);
  ScreenCaptureNotificationBlocker(const ScreenCaptureNotificationBlocker&) =
      delete;
  ScreenCaptureNotificationBlocker& operator=(
      const ScreenCaptureNotificationBlocker&) = delete;
  ~ScreenCaptureNotificationBlocker() override;

  // NotificationBlocker:
  bool ShouldBlockNotification(
      const message_center::Notification& notification) override;
  void OnBlockedNotification(const message_center::Notification& notification,
                             bool replaced) override;
  void OnClosedNotification(
      const message_center::Notification& notification) override;

  // MutedNotificationHandler::Delegate:
  void OnAction(MutedNotificationHandler::Action action) override;

  // MediaStreamCaptureIndicator::Observer:
  void OnIsCapturingDisplayChanged(content::WebContents* web_contents,
                                   bool is_capturing_display) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(ScreenCaptureNotificationBlockerTest,
                           ObservesMediaStreamCaptureIndicator);

  void ReportSessionMetrics(bool revealed);
  void ReportMuteNotificationAction(MutedNotificationHandler::Action action);
  void DisplayMuteNotification();
  void CloseMuteNotification();

  enum class NotifyState {
    // We will show "muted" notifications instead of the actual notifications.
    kNotifyMuted,
    // The user clicked on "Show" and we show all notifications as usual.
    kShowAll,
  };

  NotifyState state_ = NotifyState::kNotifyMuted;

  // Number of notifications muted during the current screen capture session.
  int muted_notification_count_ = 0;
  // Number of notifications replaced during the current screen capture session.
  int replaced_notification_count_ = 0;
  // Number of notifications closed during the current screen capture session.
  int closed_notification_count_ = 0;
  // Flag if metrics have been reported for the current screen capture session.
  bool reported_session_metrics_ = false;
  // Timestamp of when the last "muted" notification got shown.
  base::TimeTicks last_mute_notification_time_;
  // Timestamp of when the last screen capture session started.
  base::TimeTicks last_screen_capture_session_start_time_;

  // The |notification_display_service_| owns a NotificationDisplayQueue which
  // owns |this| so a raw pointer is safe here.
  NotificationDisplayService* notification_display_service_;

  ScopedObserver<MediaStreamCaptureIndicator,
                 MediaStreamCaptureIndicator::Observer>
      observer_{this};

  // Storing raw pointers here is fine because MediaStreamCaptureIndicator
  // notifies us before the WebContents is destroyed.
  base::flat_set<content::WebContents*> capturing_web_contents_;
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCREEN_CAPTURE_NOTIFICATION_BLOCKER_H_
