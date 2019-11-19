// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_MODEL_ASSISTANT_NOTIFICATION_MODEL_H_
#define ASH_ASSISTANT_MODEL_ASSISTANT_NOTIFICATION_MODEL_H_

#include <map>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "chromeos/services/assistant/public/mojom/assistant.mojom.h"

namespace ash {

class AssistantNotificationModelObserver;

// The model belonging to AssistantNotificationController which tracks
// notification state and notifies a pool of observers.
class COMPONENT_EXPORT(ASSISTANT_MODEL) AssistantNotificationModel {
 public:
  using AssistantNotification =
      chromeos::assistant::mojom::AssistantNotification;
  using AssistantNotificationPtr =
      chromeos::assistant::mojom::AssistantNotificationPtr;
  using AssistantNotificationType =
      chromeos::assistant::mojom::AssistantNotificationType;

  AssistantNotificationModel();
  ~AssistantNotificationModel();

  // Adds/removes the specified notification model |observer|.
  void AddObserver(AssistantNotificationModelObserver* observer);
  void RemoveObserver(AssistantNotificationModelObserver* observer);

  // Adds or updates the specified |notification| in the model. If there is an
  // existing notification with the same |client_id|, an update will occur.
  // Otherwise a new notification will be added.
  void AddOrUpdateNotification(AssistantNotificationPtr notification);

  // Removes the notification uniquely identified by |id|. If |from_server| is
  // true the request to remove was initiated by the server.
  void RemoveNotificationById(const std::string& id, bool from_server);

  // Removes the notifications identified by |grouping_key|. If |from_server| is
  // true the request to remove was initiated by the server.
  void RemoveNotificationsByGroupingKey(const std::string& grouping_key,
                                        bool from_server);

  // Removes all notifications. If |from_server| is true the request to remove
  // was initiated by the server.
  void RemoveAllNotifications(bool from_server);

  // Returns the notification uniquely identified by |id|.
  const AssistantNotification* GetNotificationById(const std::string& id) const;

  // Returns all notifications matching the specified |type|.
  // Use base::nullopt to return all notifications independent of their type.
  std::vector<const AssistantNotification*> GetNotificationsByType(
      base::Optional<AssistantNotificationType> type) const;

  // Returns all notifications (that have not been removed).
  std::vector<const AssistantNotification*> GetNotifications() const;

  // Returns true if the model contains a notification uniquely identified by
  // |id|, otherwise false.
  bool HasNotificationForId(const std::string& id) const;

 private:
  void NotifyNotificationAdded(const AssistantNotification* notification);
  void NotifyNotificationUpdated(const AssistantNotification* notification);
  void NotifyNotificationRemoved(const AssistantNotification* notification,
                                 bool from_server);
  void NotifyAllNotificationsRemoved(bool from_server);

  // Notifications are each mapped to their unique id.
  std::map<std::string, AssistantNotificationPtr> notifications_;

  base::ObserverList<AssistantNotificationModelObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(AssistantNotificationModel);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_MODEL_ASSISTANT_NOTIFICATION_MODEL_H_
