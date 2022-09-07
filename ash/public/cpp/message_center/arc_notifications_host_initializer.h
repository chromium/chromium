// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_MESSAGE_CENTER_ARC_NOTIFICATIONS_HOST_INITIALIZER_H_
#define ASH_PUBLIC_CPP_MESSAGE_CENTER_ARC_NOTIFICATIONS_HOST_INITIALIZER_H_

#include "ash/public/cpp/ash_public_export.h"
#include "base/observer_list_types.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace ash {

class ArcNotificationManagerBase;

class ASH_PUBLIC_EXPORT ArcNotificationsHostInitializer {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Invoked when ARC notifications instance is ready.
    virtual void OnSetArcNotificationsInstance(
        ArcNotificationManagerBase* arc_notification_manager) = 0;

    // Invoked when the ArcNotificationsHostInitializer object (the thing that
    // this observer observes) will be destroyed. In response, the observer,
    // |this|, should call "RemoveObserver(this)", whether directly or
    // indirectly (e.g. via ScopedObservation::Reset).
    virtual void OnArcNotificationInitializerDestroyed(
        ArcNotificationsHostInitializer* initializer) = 0;
  };

  static ArcNotificationsHostInitializer* Get();

  virtual void SetArcNotificationManagerInstance(
      std::unique_ptr<ArcNotificationManagerBase> manager_instance) = 0;
  virtual ArcNotificationManagerBase* GetArcNotificationManagerInstance() = 0;

  // Adds and removes an observer.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

 protected:
  ArcNotificationsHostInitializer();
  virtual ~ArcNotificationsHostInitializer();
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_MESSAGE_CENTER_ARC_NOTIFICATIONS_HOST_INITIALIZER_H_
