// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/frame/ash_frame_caption_controller.h"

#include "ash/wm/window_positioning_utils.h"
#include "ash/wm/window_state.h"
#include "ash/wm/wm_event.h"
#include "ash/wm/workspace/phantom_window_controller.h"
#include "ui/aura/window.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

AshFrameCaptionController::AshFrameCaptionController() = default;
AshFrameCaptionController::~AshFrameCaptionController() = default;

bool AshFrameCaptionController::CanSnap(aura::Window* window) {
  return wm::GetWindowState(window)->CanSnap();
}

void AshFrameCaptionController::ShowSnapPreview(aura::Window* window,
                                                mojom::SnapDirection snap) {
  if (snap == mojom::SnapDirection::kNone) {
    phantom_window_controller_.reset();
    return;
  }

  if (!phantom_window_controller_ ||
      phantom_window_controller_->window() != window) {
    phantom_window_controller_ =
        std::make_unique<PhantomWindowController>(window);
  }
  gfx::Rect phantom_bounds_in_screen =
      (snap == mojom::SnapDirection::kLeft)
          ? wm::GetDefaultLeftSnappedWindowBoundsInParent(window)
          : wm::GetDefaultRightSnappedWindowBoundsInParent(window);
  ::wm::ConvertRectToScreen(window->parent(), &phantom_bounds_in_screen);
  phantom_window_controller_->Show(phantom_bounds_in_screen);
}

void AshFrameCaptionController::CommitSnap(aura::Window* window,
                                           mojom::SnapDirection snap) {
  phantom_window_controller_.reset();
  if (snap == mojom::SnapDirection::kNone)
    return;

  wm::WindowState* window_state = wm::GetWindowState(window);
  const wm::WMEvent snap_event(snap == mojom::SnapDirection::kLeft
                                   ? wm::WM_EVENT_SNAP_LEFT
                                   : wm::WM_EVENT_SNAP_RIGHT);
  window_state->OnWMEvent(&snap_event);
}

}  // namespace ash
