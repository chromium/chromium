// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/external_arc/message_center/arc_notification_surface_manager_impl.h"

#include <string>
#include <utility>

#include "ash/public/cpp/external_arc/message_center/arc_notification_surface_impl.h"
#include "components/exo/notification_surface.h"

namespace ash {

ArcNotificationSurfaceManagerImpl::ArcNotificationSurfaceManagerImpl() =
    default;

ArcNotificationSurfaceManagerImpl::~ArcNotificationSurfaceManagerImpl() =
    default;

ArcNotificationSurface* ArcNotificationSurfaceManagerImpl::GetArcSurface(
    const std::string& notification_key) const {
  auto it = notification_surface_map_.find(notification_key);
  return it == notification_surface_map_.end() ? nullptr : it->second.get();
}

void ArcNotificationSurfaceManagerImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ArcNotificationSurfaceManagerImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

exo::NotificationSurface* ArcNotificationSurfaceManagerImpl::GetSurface(
    const std::string& notification_key) const {
  auto it = notification_surface_map_.find(notification_key);
  return it == notification_surface_map_.end() ? nullptr
                                               : it->second->surface();
}

void ArcNotificationSurfaceManagerImpl::AddSurface(
    exo::NotificationSurface* surface) {
  auto result = notification_surface_map_.insert(
      std::pair<std::string, std::unique_ptr<ArcNotificationSurfaceImpl>>(
          surface->notification_key(),
          std::make_unique<ArcNotificationSurfaceImpl>(surface)));
  if (!result.second) {
    NOTREACHED();
    return;
  }

  for (auto& observer : observers_)
    observer.OnNotificationSurfaceAdded(result.first->second.get());
}

void ArcNotificationSurfaceManagerImpl::RemoveSurface(
    exo::NotificationSurface* surface) {
  auto it = notification_surface_map_.find(surface->notification_key());
  if (it == notification_surface_map_.end())
    return;

  auto arc_surface = std::move(it->second);
  notification_surface_map_.erase(it);
  for (auto& observer : observers_)
    observer.OnNotificationSurfaceRemoved(arc_surface.get());
}

}  // namespace ash
