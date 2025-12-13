// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_MESSAGE_CENTER_ARC_NOTIFICATIONS_HOST_INITIALIZER_H_
#define ASH_PUBLIC_CPP_MESSAGE_CENTER_ARC_NOTIFICATIONS_HOST_INITIALIZER_H_

#include "ash/public/cpp/ash_public_export.h"
#include "base/observer_list_types.h"

namespace ash {

class ArcNotificationManagerBase;

class ASH_PUBLIC_EXPORT ArcNotificationsHostInitializer {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Invoked when ArcNotificationManager is initialized.
    virtual void OnArcNotificationManagerInitialized(
        ArcNotificationManagerBase* arc_notification_manager) = 0;
  };

  static ArcNotificationsHostInitializer* Get();

  virtual void SetArcNotificationManager(
      std::unique_ptr<ArcNotificationManagerBase> arc_notification_manager) = 0;
  virtual ArcNotificationManagerBase* GetArcNotificationManager() = 0;

  // Adds and removes an observer.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

 protected:
  ArcNotificationsHostInitializer();
  virtual ~ArcNotificationsHostInitializer();
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_MESSAGE_CENTER_ARC_NOTIFICATIONS_HOST_INITIALIZER_H_
