// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_display_queue.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"
#include "chrome/browser/notifications/notification_display_service.h"

namespace {

#if !defined(OS_ANDROID)
// This notification blocker listens to the events when the user starts
// capturing a display. It will block notifications while such a capture is
// ongoing. Note that this does not include casting the whole display and only
// covers capturing via WebContents.
class ScreenCaptureNotificationBlocker
    : public NotificationBlocker,
      public MediaStreamCaptureIndicator::Observer {
 public:
  ScreenCaptureNotificationBlocker() {
    observer_.Add(MediaCaptureDevicesDispatcher::GetInstance()
                      ->GetMediaStreamCaptureIndicator()
                      .get());
  }

  // NotificationBlocker:
  bool ShouldBlockNotifications() override {
    return !capturing_web_contents_.empty();
  }

  // MediaStreamCaptureIndicator::Observer:
  void OnIsCapturingDisplayChanged(content::WebContents* web_contents,
                                   bool is_capturing_display) override {
    if (is_capturing_display)
      capturing_web_contents_.insert(web_contents);
    else
      capturing_web_contents_.erase(web_contents);

    NotifyBlockingStateChanged();
  }

 private:
  ScopedObserver<MediaStreamCaptureIndicator,
                 MediaStreamCaptureIndicator::Observer>
      observer_{this};

  // Storing raw pointers here is fine because we never access them. We're only
  // interested in the current set of WebContents that captures a display.
  base::flat_set<content::WebContents*> capturing_web_contents_;
};
#endif  // !defined(OS_ANDROID)

}  // namespace

NotificationDisplayQueue::NotificationDisplayQueue(
    NotificationDisplayService* notification_display_service)
    : notification_display_service_(notification_display_service) {
  NotificationBlockers blockers;

#if !defined(OS_ANDROID)
  // TODO(knollr): Also block notifications while casting a screen.
  if (base::FeatureList::IsEnabled(
          features::kMuteNotificationsDuringScreenShare)) {
    blockers.push_back(std::make_unique<ScreenCaptureNotificationBlocker>());
  }
#endif  // !defined(OS_ANDROID)

  SetNotificationBlockers(std::move(blockers));
}

NotificationDisplayQueue::~NotificationDisplayQueue() = default;

void NotificationDisplayQueue::OnBlockingStateChanged() {
  MaybeDisplayQueuedNotifications();
}

bool NotificationDisplayQueue::ShouldEnqueueNotifications() {
  return std::any_of(blockers_.begin(), blockers_.end(),
                     [](const std::unique_ptr<NotificationBlocker>& blocker) {
                       return blocker->ShouldBlockNotifications();
                     });
}

void NotificationDisplayQueue::EnqueueNotification(
    NotificationHandler::Type notification_type,
    const message_center::Notification& notification,
    std::unique_ptr<NotificationCommon::Metadata> metadata) {
  RemoveQueuedNotification(notification.id());
  queued_notifications_.emplace_back(notification_type, notification,
                                     std::move(metadata));
}

void NotificationDisplayQueue::RemoveQueuedNotification(
    const std::string& notification_id) {
  auto it =
      std::find_if(queued_notifications_.begin(), queued_notifications_.end(),
                   [&notification_id](const QueuedNotification& queued) {
                     return queued.notification.id() == notification_id;
                   });

  if (it != queued_notifications_.end())
    queued_notifications_.erase(it);
}

std::set<std::string> NotificationDisplayQueue::GetQueuedNotificationIds() {
  std::set<std::string> notification_ids;
  for (const QueuedNotification& queued : queued_notifications_)
    notification_ids.insert(queued.notification.id());

  return notification_ids;
}

void NotificationDisplayQueue::SetNotificationBlockers(
    NotificationBlockers blockers) {
  // Remove old blockers from the observer.
  for (auto& blocker : blockers_)
    notification_blocker_observer_.Remove(blocker.get());

  // Add new blockers and observe them.
  blockers_ = std::move(blockers);
  for (auto& blocker : blockers_)
    notification_blocker_observer_.Add(blocker.get());

  // Update new state with new blockers.
  MaybeDisplayQueuedNotifications();
}

void NotificationDisplayQueue::MaybeDisplayQueuedNotifications() {
  if (ShouldEnqueueNotifications())
    return;

  std::vector<QueuedNotification> queued_notifications =
      std::move(queued_notifications_);
  queued_notifications_.clear();

  for (QueuedNotification& queued : queued_notifications) {
    notification_display_service_->Display(queued.notification_type,
                                           queued.notification,
                                           std::move(queued.metadata));
  }
}

NotificationDisplayQueue::QueuedNotification::QueuedNotification(
    NotificationHandler::Type notification_type,
    const message_center::Notification& notification,
    std::unique_ptr<NotificationCommon::Metadata> metadata)
    : notification_type(notification_type),
      notification(notification),
      metadata(std::move(metadata)) {}

NotificationDisplayQueue::QueuedNotification::QueuedNotification(
    QueuedNotification&&) = default;

NotificationDisplayQueue::QueuedNotification&
NotificationDisplayQueue::QueuedNotification::operator=(QueuedNotification&&) =
    default;

NotificationDisplayQueue::QueuedNotification::~QueuedNotification() = default;
