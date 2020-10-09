// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_display_queue.h"

#include <algorithm>
#include <utility>

#include "chrome/browser/notifications/notification_display_service.h"

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

bool NotificationDisplayQueue::ShouldEnqueueNotifications(
    NotificationHandler::Type notification_type) const {
  return IsWebNotification(notification_type) &&
         IsAnyNotificationBlockerActive();
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

std::set<std::string> NotificationDisplayQueue::GetQueuedNotificationIds()
    const {
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

void NotificationDisplayQueue::AddNotificationBlocker(
    std::unique_ptr<NotificationBlocker> blocker) {
  notification_blocker_observer_.Add(blocker.get());
  blockers_.push_back(std::move(blocker));
}

void NotificationDisplayQueue::MaybeDisplayQueuedNotifications() {
  if (IsAnyNotificationBlockerActive())
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

bool NotificationDisplayQueue::IsAnyNotificationBlockerActive() const {
  return std::any_of(blockers_.begin(), blockers_.end(),
                     [](const std::unique_ptr<NotificationBlocker>& blocker) {
                       return blocker->ShouldBlockNotifications();
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
