// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_MESSAGE_CENTER_ARC_NOTIFICATION_MANAGER_BASE_H_
#define ASH_PUBLIC_CPP_MESSAGE_CENTER_ARC_NOTIFICATION_MANAGER_BASE_H_

#include <string>

#include "ash/public/cpp/ash_public_export.h"
#include "base/observer_list_types.h"

class AccountId;

namespace message_center {
class MessageCenter;
}

namespace ash {
class ArcNotificationManagerDelegate;

// Interface for ash ArcNotificationManager.
class ASH_PUBLIC_EXPORT ArcNotificationManagerBase {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Invoked when the notification for the given |notification_id| is posted
    // or updated.
    virtual void OnNotificationUpdated(const std::string& notification_id,
                                       const std::string& app_id) = 0;

    // Invoked when the notification for the given |notification_id| is removed.
    virtual void OnNotificationRemoved(const std::string& notification_id) = 0;

    // Invoked when the ArcNotificationManagerBase object (the thing that this
    // observer observes) will be destroyed. In response, the observer, |this|,
    // should call "RemoveObserver(this)", whether directly or indirectly (e.g.
    // via ScopedObservation::Reset).
    virtual void OnArcNotificationManagerDestroyed(
        ArcNotificationManagerBase* arc_notification_manager) = 0;
  };

  ArcNotificationManagerBase();
  virtual ~ArcNotificationManagerBase();

  ArcNotificationManagerBase(const ArcNotificationManagerBase&) = delete;
  ArcNotificationManagerBase& operator=(const ArcNotificationManagerBase&) =
      delete;

  virtual void Init(std::unique_ptr<ArcNotificationManagerDelegate> delegate,
                    const AccountId& profile_id,
                    message_center::MessageCenter* message_center) = 0;
  // Adds and removes an observer.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_MESSAGE_CENTER_ARC_NOTIFICATION_MANAGER_BASE_H_
