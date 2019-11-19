// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/model/assistant_notification_model.h"

#include "ash/assistant/model/assistant_notification_model_observer.h"
#include "base/stl_util.h"

namespace ash {

AssistantNotificationModel::AssistantNotificationModel() = default;

AssistantNotificationModel::~AssistantNotificationModel() = default;

void AssistantNotificationModel::AddObserver(
    AssistantNotificationModelObserver* observer) {
  observers_.AddObserver(observer);
}

void AssistantNotificationModel::RemoveObserver(
    AssistantNotificationModelObserver* observer) {
  observers_.RemoveObserver(observer);
}

void AssistantNotificationModel::AddOrUpdateNotification(
    AssistantNotificationPtr notification) {
  AssistantNotification* ptr = notification.get();

  DCHECK(!ptr->client_id.empty());
  bool is_update = HasNotificationForId(ptr->client_id);

  notifications_[ptr->client_id] = std::move(notification);

  if (is_update)
    NotifyNotificationUpdated(ptr);
  else
    NotifyNotificationAdded(ptr);
}

void AssistantNotificationModel::RemoveNotificationById(const std::string& id,
                                                        bool from_server) {
  auto it = notifications_.find(id);
  if (it == notifications_.end())
    return;

  AssistantNotificationPtr notification = std::move(it->second);
  notifications_.erase(id);
  NotifyNotificationRemoved(notification.get(), from_server);
}

void AssistantNotificationModel::RemoveNotificationsByGroupingKey(
    const std::string& grouping_key,
    bool from_server) {
  for (auto it = notifications_.begin(); it != notifications_.end();) {
    if (it->second->grouping_key == grouping_key) {
      AssistantNotificationPtr notification =
          std::move(notifications_[it->second->client_id]);
      it = notifications_.erase(it);
      NotifyNotificationRemoved(notification.get(), from_server);
      continue;
    }
    ++it;
  }
}

void AssistantNotificationModel::RemoveAllNotifications(bool from_server) {
  if (notifications_.empty())
    return;

  notifications_.clear();
  NotifyAllNotificationsRemoved(from_server);
}

const chromeos::assistant::mojom::AssistantNotification*
AssistantNotificationModel::GetNotificationById(const std::string& id) const {
  auto it = notifications_.find(id);
  return it != notifications_.end() ? it->second.get() : nullptr;
}

std::vector<const chromeos::assistant::mojom::AssistantNotification*>
AssistantNotificationModel::GetNotifications() const {
  return GetNotificationsByType(base::nullopt);
}

std::vector<const chromeos::assistant::mojom::AssistantNotification*>
AssistantNotificationModel::GetNotificationsByType(
    base::Optional<AssistantNotificationType> type) const {
  std::vector<const AssistantNotification*> notifications;
  for (const auto& notification : notifications_) {
    if (!type || notification.second->type == type.value())
      notifications.push_back(notification.second.get());
  }
  return notifications;
}

bool AssistantNotificationModel::HasNotificationForId(
    const std::string& id) const {
  return base::Contains(notifications_, id);
}

void AssistantNotificationModel::NotifyNotificationAdded(
    const AssistantNotification* notification) {
  for (auto& observer : observers_)
    observer.OnNotificationAdded(notification);
}

void AssistantNotificationModel::NotifyNotificationUpdated(
    const AssistantNotification* notification) {
  for (auto& observer : observers_)
    observer.OnNotificationUpdated(notification);
}

void AssistantNotificationModel::NotifyNotificationRemoved(
    const AssistantNotification* notification,
    bool from_server) {
  for (auto& observer : observers_)
    observer.OnNotificationRemoved(notification, from_server);
}

void AssistantNotificationModel::NotifyAllNotificationsRemoved(
    bool from_server) {
  for (auto& observer : observers_)
    observer.OnAllNotificationsRemoved(from_server);
}

}  // namespace ash
