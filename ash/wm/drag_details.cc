// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/drag_details.h"

#include "ash/public/cpp/window_properties.h"
#include "ash/wm/window_resizer.h"
#include "ui/aura/window.h"
#include "ui/base/hit_test.h"
#include "ui/display/screen.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

namespace {

int GetSizeChangeDirectionForWindowComponent(int window_component) {
  int size_change_direction = WindowResizer::kBoundsChangeDirection_None;
  switch (window_component) {
    case HTTOPLEFT:
    case HTTOPRIGHT:
    case HTBOTTOMLEFT:
    case HTBOTTOMRIGHT:
    case HTGROWBOX:
    case HTCAPTION:
      size_change_direction |=
          WindowResizer::kBoundsChangeDirection_Horizontal |
          WindowResizer::kBoundsChangeDirection_Vertical;
      break;
    case HTTOP:
    case HTBOTTOM:
      size_change_direction |= WindowResizer::kBoundsChangeDirection_Vertical;
      break;
    case HTRIGHT:
    case HTLEFT:
      size_change_direction |= WindowResizer::kBoundsChangeDirection_Horizontal;
      break;
    default:
      break;
  }
  return size_change_direction;
}

gfx::Rect GetWindowInitialBoundsInParent(aura::Window* window) {
  // Floated windows should use their current bounds as a starting point, even
  // in tablet mode.
  if (WindowState::Get(window)->IsFloated())
    return window->bounds();

  if (display::Screen::GetScreen()->InTabletMode()) {
    gfx::Rect* override_bounds = window->GetProperty(kRestoreBoundsOverrideKey);
    if (override_bounds && !override_bounds->IsEmpty()) {
      wm::ConvertRectFromScreen(window->GetRootWindow(), override_bounds);
      return *override_bounds;
    }
  }
  return window->bounds();
}

gfx::Rect GetRestoreBoundsInParent(aura::Window* window, int window_component) {
  if (window_component != HTCAPTION)
    return gfx::Rect();

  WindowState* window_state = WindowState::Get(window);

  // TODO(xdai): Move these logic to WindowState::GetRestoreBoundsInScreen()
  // and let it return the right value.
  gfx::Rect restore_bounds;
  if (display::Screen::GetScreen()->InTabletMode()) {
    gfx::Rect* override_bounds = window->GetProperty(kRestoreBoundsOverrideKey);
    if (override_bounds && !override_bounds->IsEmpty()) {
      restore_bounds = *override_bounds;
      wm::ConvertRectFromScreen(window->parent(), &restore_bounds);
    }
  } else if (window_state->IsSnapped() || window_state->IsMaximized()) {
    // For client controlled windows, the client-side controls the restore
    // bounds, so it is not always available on ash-side.
    DCHECK(window_state->is_client_controlled() ||
           window_state->HasRestoreBounds());
    restore_bounds = window_state->GetRestoreBoundsInParent();
  } else if ((window_state->IsNormalStateType() || window_state->IsFloated()) &&
             window_state->HasRestoreBounds()) {
    restore_bounds = window_state->GetRestoreBoundsInParent();
  }
  return restore_bounds;
}

}  // namespace

DragDetails::DragDetails(aura::Window* window,
                         const gfx::PointF& location,
                         int window_component,
                         wm::WindowMoveSource source)
    : initial_state_type(WindowState::Get(window)->GetStateType()),
      initial_bounds_in_parent(GetWindowInitialBoundsInParent(window)),
      restore_bounds_in_parent(
          GetRestoreBoundsInParent(window, window_component)),
      initial_location_in_parent(location),
      window_component(window_component),
      bounds_change(
          WindowResizer::GetBoundsChangeForWindowComponent(window_component)),
      size_change_direction(
          GetSizeChangeDirectionForWindowComponent(window_component)),
      is_resizable(bounds_change != WindowResizer::kBoundsChangeDirection_None),
      source(source) {}

DragDetails::~DragDetails() = default;

}  // namespace ash
