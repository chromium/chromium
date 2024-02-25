// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_display_queue.h"

#include <utility>

#include "base/ranges/algorithm.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "url/origin.h"

namespace {

bool IsWebNotification(NotificationHandler::Type notification_type) {
  return notification_type == NotificationHandler::Type::WEB_PERSISTENT ||
         notification_type == NotificationHandler::Type::WEB_NON_PERSISTENT ||
         notification_type == NotificationHandler::Type::EXTENSION;
}

}  // namespace

NotificationDisplayQueue::NotificationDisplayQueue(
    NotificationDisplayService* notification_display_service)
    : notification_display_service_(notification_display_service) {}

NotificationDisplayQueue::~NotificationDisplayQueue() = default;

void NotificationDisplayQueue::OnBlockingStateChanged() {
  MaybeDisplayQueuedNotifications();
}

bool NotificationDisplayQueue::ShouldEnqueueNotification(
    NotificationHandler::Type notification_type,
    const message_center::Notification& notification) const {
  return IsWebNotification(notification_type) &&
         IsAnyNotificationBlockerActive(notification);
}

void NotificationDisplayQueue::EnqueueNotification(
    NotificationHandler::Type notification_type,
    const message_center::Notification& notification,
    std::unique_ptr<NotificationCommon::Metadata> metadata) {
  bool replaced =
      DoRemoveQueuedNotification(notification.id(), /*notify=*/false);
  queued_notifications_.emplace_back(notification_type, notification,
                                     std::move(metadata));
  // Notify blockers that a new notification has been blocked.
  for (auto& blocker : blockers_) {
    if (blocker->ShouldBlockNotification(notification))
      blocker->OnBlockedNotification(notification, replaced);
  }
}

void NotificationDisplayQueue::RemoveQueuedNotification(
    const std::string& notification_id) {
  DoRemoveQueuedNotification(notification_id, /*notify=*/true);
}

std::set<std::string> NotificationDisplayQueue::GetQueuedNotificationIds()
    const {
  std::set<std::string> notification_ids;
  for (const QueuedNotification& queued : queued_notifications_) {
    notification_ids.insert(queued.notification.id());
  }

  return notification_ids;
}

std::set<std::string>
NotificationDisplayQueue::GetQueuedNotificationIdsForOrigin(
    const GURL& origin) const {
  std::set<std::string> notification_ids;
  for (const QueuedNotification& queued : queued_notifications_) {
    if (url::IsSameOriginWith(queued.notification.origin_url(), origin)) {
      notification_ids.insert(queued.notification.id());
    }
  }

  return notification_ids;
}

void NotificationDisplayQueue::SetNotificationBlockers(
    NotificationBlockers blockers) {
  // Remove old blockers from the observer.
  for (auto& blocker : blockers_)
    notification_blocker_observations_.RemoveObservation(blocker.get());

  // Add new blockers and observe them.
  blockers_ = std::move(blockers);
  for (auto& blocker : blockers_)
    notification_blocker_observations_.AddObservation(blocker.get());

  // Update new state with new blockers.
  MaybeDisplayQueuedNotifications();
}

void NotificationDisplayQueue::AddNotificationBlocker(
    std::unique_ptr<NotificationBlocker> blocker) {
  notification_blocker_observations_.AddObservation(blocker.get());
  blockers_.push_back(std::move(blocker));
}

bool NotificationDisplayQueue::DoRemoveQueuedNotification(
    const std::string& notification_id,
    bool notify) {
  auto it = base::ranges::find(queued_notifications_, notification_id,
                               [](const QueuedNotification& queued) {
                                 return queued.notification.id();
                               });

  if (it == queued_notifications_.end())
    return false;

  if (notify) {
    for (auto& blocker : blockers_) {
      if (blocker->ShouldBlockNotification(it->notification))
        blocker->OnClosedNotification(it->notification);
    }
  }

  queued_notifications_.erase(it);
  return true;
}

void NotificationDisplayQueue::MaybeDisplayQueuedNotifications() {
  auto show_begin = std::stable_partition(
      queued_notifications_.begin(), queued_notifications_.end(),
      [&](const QueuedNotification& queued) {
        return IsAnyNotificationBlockerActive(queued.notification);
      });

  std::vector<QueuedNotification> notifications;
  notifications.insert(notifications.end(), std::make_move_iterator(show_begin),
                       std::make_move_iterator(queued_notifications_.end()));

  queued_notifications_.erase(show_begin, queued_notifications_.end());

  for (QueuedNotification& queued : notifications) {
    notification_display_service_->Display(queued.notification_type,
                                           queued.notification,
                                           std::move(queued.metadata));
  }
}

bool NotificationDisplayQueue::IsAnyNotificationBlockerActive(
    const message_center::Notification& notification) const {
  return base::ranges::any_of(
      blockers_,
      [&notification](const std::unique_ptr<NotificationBlocker>& blocker) {
        return blocker->ShouldBlockNotification(notification);
      });
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
