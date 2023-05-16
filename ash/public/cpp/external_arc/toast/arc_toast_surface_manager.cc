// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/external_arc/toast/arc_toast_surface_manager.h"

#include "base/ranges/algorithm.h"
#include "components/exo/toast_surface.h"

namespace ash {

ArcToastSurfaceManager::ArcToastSurfaceManager()
    : locked_(ash::SessionController::Get()->IsScreenLocked()) {
  scoped_observation_.Observe(ash::SessionController::Get());
}

ArcToastSurfaceManager::~ArcToastSurfaceManager() = default;

void ArcToastSurfaceManager::AddSurface(exo::ToastSurface* surface) {
  toast_surfaces_.push_back(surface);
  UpdateVisibility();
}

void ArcToastSurfaceManager::RemoveSurface(exo::ToastSurface* surface) {
  auto it = base::ranges::find(toast_surfaces_, surface);
  DLOG_IF(ERROR, it == toast_surfaces_.end())
      << "Can't remove not registered surface";

  if (it != toast_surfaces_.end())
    toast_surfaces_.erase(it);
}

void ArcToastSurfaceManager::UpdateVisibility() {
  for (exo::ToastSurface* surface : toast_surfaces_) {
    if (!surface->GetWidget())
      continue;

    if (locked_)
      surface->GetWidget()->Hide();
    else
      surface->GetWidget()->Show();
  }
}

void ArcToastSurfaceManager::OnSessionStateChanged(
    session_manager::SessionState state) {
  const bool locked = state != session_manager::SessionState::ACTIVE;
  if (locked != locked_) {
    locked_ = locked;
    UpdateVisibility();
  }
}

}  // namespace ash
