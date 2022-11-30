// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/external_arc/keyboard/arc_input_method_surface_manager.h"
#include "components/exo/input_method_surface.h"

namespace ash {

exo::InputMethodSurface* ArcInputMethodSurfaceManager::GetSurface() const {
  return input_method_surface_;
}

ArcInputMethodSurfaceManager::ArcInputMethodSurfaceManager() = default;
ArcInputMethodSurfaceManager::~ArcInputMethodSurfaceManager() = default;

void ArcInputMethodSurfaceManager::AddSurface(
    exo::InputMethodSurface* surface) {
  DCHECK_EQ(input_method_surface_, nullptr);
  input_method_surface_ = surface;
}

void ArcInputMethodSurfaceManager::RemoveSurface(
    exo::InputMethodSurface* surface) {
  DLOG_IF(ERROR, input_method_surface_ != surface)
      << "Can't remove not registered surface";

  if (input_method_surface_ == surface)
    input_method_surface_ = nullptr;

  NotifyArcInputMethodBoundsChanged(gfx::Rect());
}

void ArcInputMethodSurfaceManager::OnTouchableBoundsChanged(
    exo::InputMethodSurface* surface) {
  DLOG_IF(ERROR, input_method_surface_ != surface)
      << "OnTouchableBoundsChanged is called for not registered surface";
  NotifyArcInputMethodBoundsChanged(surface->GetBounds());
}

}  // namespace ash
