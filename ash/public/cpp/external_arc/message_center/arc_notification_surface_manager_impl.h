// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_EXTERNAL_ARC_MESSAGE_CENTER_ARC_NOTIFICATION_SURFACE_MANAGER_IMPL_H_
#define ASH_PUBLIC_CPP_EXTERNAL_ARC_MESSAGE_CENTER_ARC_NOTIFICATION_SURFACE_MANAGER_IMPL_H_

#include <map>
#include <string>
#include <unordered_map>

#include "ash/public/cpp/external_arc/message_center/arc_notification_surface_manager.h"
#include "base/observer_list.h"
#include "components/exo/notification_surface_manager.h"

namespace ash {

class ArcNotificationSurface;
class ArcNotificationSurfaceImpl;

class ArcNotificationSurfaceManagerImpl
    : public ArcNotificationSurfaceManager,
      public exo::NotificationSurfaceManager {
 public:
  ArcNotificationSurfaceManagerImpl();

  ArcNotificationSurfaceManagerImpl(const ArcNotificationSurfaceManagerImpl&) =
      delete;
  ArcNotificationSurfaceManagerImpl& operator=(
      const ArcNotificationSurfaceManagerImpl&) = delete;

  ~ArcNotificationSurfaceManagerImpl() override;

  // ArcNotificationSurfaceManager:
  ArcNotificationSurface* GetArcSurface(
      const std::string& notification_id) const override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  void OnNotificationSurfaceAXTreeIdChanged(
      ArcNotificationSurface* surface) override;

  // exo::NotificationSurfaceManager:
  exo::NotificationSurface* GetSurface(
      const std::string& notification_id) const override;
  void AddSurface(exo::NotificationSurface* surface) override;
  void RemoveSurface(exo::NotificationSurface* surface) override;

 private:
  using NotificationSurfaceMap =
      std::unordered_map<std::string,
                         std::unique_ptr<ArcNotificationSurfaceImpl>>;

  void RemoveSurfaceByKey(const std::string& notification_key);

  NotificationSurfaceMap notification_surface_map_;

  base::ObserverList<Observer>::Unchecked observers_;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_EXTERNAL_ARC_MESSAGE_CENTER_ARC_NOTIFICATION_SURFACE_MANAGER_IMPL_H_
