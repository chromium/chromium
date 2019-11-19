// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/drag_details.h"

#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_resizer.h"
#include "ui/aura/window.h"
#include "ui/base/hit_test.h"
#include "ui/compositor/layer.h"
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
  const bool is_tablet_mode =
      Shell::Get()->tablet_mode_controller()->InTabletMode();
  if (is_tablet_mode) {
    gfx::Rect* override_bounds = window->GetProperty(kRestoreBoundsOverrideKey);
    if (override_bounds && !override_bounds->IsEmpty()) {
      gfx::Rect bounds = *override_bounds;
      ::wm::ConvertRectFromScreen(window->GetRootWindow(), &bounds);
      return bounds;
    }
  }
  return window->bounds();
}

}  // namespace

DragDetails::DragDetails(aura::Window* window,
                         const gfx::Point& location,
                         int window_component,
                         ::wm::WindowMoveSource source)
    : initial_state_type(WindowState::Get(window)->GetStateType()),
      initial_bounds_in_parent(GetWindowInitialBoundsInParent(window)),
      initial_location_in_parent(location),
      // When drag starts, we might be in the middle of a window opacity
      // animation, on drag completion we must set the opacity to the target
      // opacity rather than the current opacity (crbug.com/687003).
      initial_opacity(window->layer()->GetTargetOpacity()),
      window_component(window_component),
      bounds_change(
          WindowResizer::GetBoundsChangeForWindowComponent(window_component)),
      position_change_direction(
          WindowResizer::GetPositionChangeDirectionForWindowComponent(
              window_component)),
      size_change_direction(
          GetSizeChangeDirectionForWindowComponent(window_component)),
      is_resizable(bounds_change != WindowResizer::kBoundsChangeDirection_None),
      source(source) {
  if (window_component != HTCAPTION)
    return;

  WindowState* window_state = WindowState::Get(window);
  const bool is_tablet_mode =
      Shell::Get()->tablet_mode_controller()->InTabletMode();
  // TODO(xdai): Move these logic to WindowState::GetRestoreBoundsInScreen()
  // and let it return the right value.
  if (!is_tablet_mode && window_state->IsNormalOrSnapped() &&
      window_state->HasRestoreBounds()) {
    restore_bounds = window_state->GetRestoreBoundsInScreen();
  } else if (is_tablet_mode) {
    gfx::Rect* override_bounds = window->GetProperty(kRestoreBoundsOverrideKey);
    if (override_bounds && !override_bounds->IsEmpty())
      restore_bounds = *override_bounds;
  }
}

DragDetails::~DragDetails() = default;

}  // namespace ash
