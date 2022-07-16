// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_restore/window_restore_util.h"

#include "ash/public/cpp/app_types_util.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/window_state.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

std::unique_ptr<app_restore::WindowInfo> BuildWindowInfo(
    aura::Window* window,
    absl::optional<int> activation_index,
    const std::vector<aura::Window*>& mru_windows) {
  auto window_info = std::make_unique<app_restore::WindowInfo>();
  int window_activation_index = -1;
  if (activation_index) {
    window_activation_index = *activation_index;
  } else {
    auto it = std::find(mru_windows.begin(), mru_windows.end(), window);
    if (it != mru_windows.end())
      window_activation_index = it - mru_windows.begin();
  }
  if (window_activation_index != -1)
    window_info->activation_index = window_activation_index;
  window_info->window = window;
  window_info->desk_id = window->GetProperty(aura::client::kWindowWorkspaceKey);

  // If override bounds and window state are available (in tablet mode), save
  // those bounds.
  gfx::Rect* override_bounds = window->GetProperty(kRestoreBoundsOverrideKey);
  WindowState* window_state = WindowState::Get(window);
  if (override_bounds) {
    window_info->current_bounds = *override_bounds;
    // Snapped state can be restored from tablet onto clamshell, so we do not
    // use the restore override state here.
    window_info->window_state_type =
        window_state->IsSnapped()
            ? window_state->GetStateType()
            : window->GetProperty(kRestoreWindowStateTypeOverrideKey);
  } else {
    // If there are restore bounds, use those as current bounds. On restore, for
    // states with restore bounds (maximized, minimized, snapped, etc), they
    // will take the current bounds as their restore bounds and have the current
    // bounds determined by the system.
    if (window_state->HasRestoreBounds()) {
      window_info->current_bounds = window_state->GetRestoreBoundsInScreen();
    } else {
      // `aura::Window::Get*Bounds*` is affected by transforms, which may be the
      // case when in overview mode. Compute the bounds in screen minus the
      // transform.
      gfx::Rect untransformed_window_bounds = window->bounds();
      wm::ConvertRectToScreen(window->parent(), &untransformed_window_bounds);
      window_info->current_bounds = untransformed_window_bounds;
    }

    // Window restore does not support restoring fullscreen windows. If a window
    // is fullscreen save the pre-fullscreen window state instead.
    window_info->window_state_type =
        window_state->IsFullscreen()
            ? chromeos::ToWindowStateType(
                  window->GetProperty(aura::client::kPreFullscreenShowStateKey))
            : window_state->GetStateType();
  }

  // Populate the pre minimized show state field if the window is minimized.
  if (window_state->IsMinimized()) {
    window_info->pre_minimized_show_state_type =
        window->GetProperty(aura::client::kPreMinimizedShowStateKey);
  }

  window_info->display_id =
      display::Screen::GetScreen()->GetDisplayNearestWindow(window).id();

  // Save window size restriction of ARC app window.
  if (IsArcWindow(window)) {
    views::Widget* widget = views::Widget::GetWidgetForNativeWindow(window);
    if (widget) {
      auto extra = app_restore::WindowInfo::ArcExtraInfo();
      extra.maximum_size = widget->GetMaximumSize();
      extra.minimum_size = widget->GetMinimumSize();
      extra.title = window->GetTitle();
      extra.bounds_in_root = window->GetBoundsInRootWindow();
      window_info->arc_extra_info = extra;
    }
  }

  return window_info;
}

}  // namespace ash
