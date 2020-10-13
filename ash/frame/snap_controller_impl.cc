// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/frame/snap_controller_impl.h"

#include "ash/wm/window_positioning_utils.h"
#include "ash/wm/window_state.h"
#include "ash/wm/wm_event.h"
#include "ash/wm/workspace/phantom_window_controller.h"
#include "ui/aura/window.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

SnapControllerImpl::SnapControllerImpl() = default;
SnapControllerImpl::~SnapControllerImpl() = default;

bool SnapControllerImpl::CanSnap(aura::Window* window) {
  return WindowState::Get(window)->CanSnap();
}

void SnapControllerImpl::ShowSnapPreview(aura::Window* window,
                                         SnapDirection snap) {
  if (snap == SnapDirection::kNone) {
    phantom_window_controller_.reset();
    return;
  }

  if (!phantom_window_controller_ ||
      phantom_window_controller_->window() != window) {
    phantom_window_controller_ =
        std::make_unique<PhantomWindowController>(window);
  }
  gfx::Rect phantom_bounds_in_screen =
      (snap == SnapDirection::kLeft)
          ? GetDefaultLeftSnappedWindowBoundsInParent(window)
          : GetDefaultRightSnappedWindowBoundsInParent(window);
  ::wm::ConvertRectToScreen(window->parent(), &phantom_bounds_in_screen);
  phantom_window_controller_->Show(phantom_bounds_in_screen);
}

void SnapControllerImpl::CommitSnap(aura::Window* window, SnapDirection snap) {
  phantom_window_controller_.reset();
  if (snap == SnapDirection::kNone)
    return;

  WindowState* window_state = WindowState::Get(window);
  const WMEvent snap_event(snap == SnapDirection::kLeft ? WM_EVENT_SNAP_LEFT
                                                        : WM_EVENT_SNAP_RIGHT);
  window_state->OnWMEvent(&snap_event);
}

}  // namespace ash
