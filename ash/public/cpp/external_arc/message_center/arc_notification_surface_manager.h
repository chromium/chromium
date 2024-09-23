// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_EXTERNAL_ARC_MESSAGE_CENTER_ARC_NOTIFICATION_SURFACE_MANAGER_H_
#define ASH_PUBLIC_CPP_EXTERNAL_ARC_MESSAGE_CENTER_ARC_NOTIFICATION_SURFACE_MANAGER_H_

#include <string>

namespace ash {

class ArcNotificationSurface;

// Keeps track of NotificationSurface.
class ArcNotificationSurfaceManager {
 public:
  class Observer {
   public:
    // Invoked when a notification surface is added to the registry.
    virtual void OnNotificationSurfaceAdded(
        ArcNotificationSurface* surface) = 0;

    // Invoked when a notification surface is removed from the registry.
    virtual void OnNotificationSurfaceRemoved(
        ArcNotificationSurface* surface) = 0;

    // Invoked when `ax_tree_id_` of notification surface changes.
    virtual void OnNotificationSurfaceAXTreeIdChanged(
        ArcNotificationSurface* surface) {}

   protected:
    virtual ~Observer() = default;
  };
  static ArcNotificationSurfaceManager* Get();

  ArcNotificationSurfaceManager(const ArcNotificationSurfaceManager&) = delete;
  ArcNotificationSurfaceManager& operator=(
      const ArcNotificationSurfaceManager&) = delete;

  virtual ~ArcNotificationSurfaceManager();

  virtual ArcNotificationSurface* GetArcSurface(
      const std::string& notification_id) const = 0;
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;
  virtual void OnNotificationSurfaceAXTreeIdChanged(
      ArcNotificationSurface* surface) {}

 protected:
  ArcNotificationSurfaceManager();

 private:
  static ArcNotificationSurfaceManager* instance_;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_EXTERNAL_ARC_MESSAGE_CENTER_ARC_NOTIFICATION_SURFACE_MANAGER_H_
