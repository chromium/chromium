// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_restore/window_restore_util.h"

#include "ash/public/cpp/app_types_util.h"
#include "ash/public/cpp/saved_desk_delegate.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/window_state.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/ranges/algorithm.h"
#include "components/app_constants/constants.h"
#include "components/app_restore/window_properties.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/window.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

namespace {

base::FilePath informed_restore_image_path_for_test_;

// If `use_screen` is true we convert to screen coordinates, otherwise we
// convert to root window coordinates.
gfx::Rect GetBoundsIgnoringTransforms(const aura::Window* window,
                                      bool use_screen) {
  // `aura::Window::Get*Bounds*` is affected by transforms, which may be the
  // case when in overview mode. Compute the bounds in screen minus the
  // transform.
  auto* client = aura::client::GetScreenPositionClient(window->GetRootWindow());
  DCHECK(client);
  gfx::Point origin;
  if (use_screen) {
    client->ConvertPointToScreenIgnoringTransforms(window, &origin);
  } else {
    client->ConvertPointToRootWindowIgnoringTransforms(window, &origin);
  }
  return gfx::Rect(origin, window->bounds().size());
}

}  // namespace

std::unique_ptr<app_restore::WindowInfo> BuildWindowInfo(
    aura::Window* window,
    std::optional<int> activation_index,
    const std::vector<raw_ptr<aura::Window, VectorExperimental>>& mru_windows) {
  auto window_info = std::make_unique<app_restore::WindowInfo>();
  int window_activation_index = -1;
  if (activation_index) {
    window_activation_index = *activation_index;
  } else {
    auto it = base::ranges::find(mru_windows, window);
    if (it != mru_windows.end())
      window_activation_index = it - mru_windows.begin();
  }
  if (window_activation_index != -1)
    window_info->activation_index = window_activation_index;
  window_info->window = window;

  // Set either the `desk_id` or set the `desk_guid`, but not both.
  const int desk_id = window->GetProperty(aura::client::kWindowWorkspaceKey);
  if (desk_id == aura::client::kWindowWorkspaceVisibleOnAllWorkspaces) {
    window_info->desk_id = desk_id;
  } else {
    const std::string* desk_uuid =
        window->GetProperty(aura::client::kDeskUuidKey);
    // It's possible for the desk to no longer exist or not be found in the case
    // of CloseAll.
    window_info->desk_guid =
        desk_uuid ? base::Uuid::ParseLowercase(*desk_uuid) : base::Uuid();
  }

  // If override bounds and window state are available (in tablet mode), save
  // those bounds.
  gfx::Rect* override_bounds = window->GetProperty(kRestoreBoundsOverrideKey);
  WindowState* window_state = WindowState::Get(window);
  if (override_bounds) {
    window_info->current_bounds = *override_bounds;
    // Snapped and floated states can be restored from tablet onto clamshell, so
    // we do not use the restore override state here.
    window_info->window_state_type =
        window_state->IsSnapped() || window_state->IsFloated()
            ? window_state->GetStateType()
            : window->GetProperty(kRestoreWindowStateTypeOverrideKey);
  } else {
    // If there are restore bounds, use those as current bounds. On restore, for
    // states with restore bounds (maximized, minimized, snapped, etc), they
    // will take the current bounds as their restore bounds and have the current
    // bounds determined by the system.
    // Note that for floated state, the window should be restored to its current
    // floated bounds since it's not stored in restore bounds.
    if (window_state->HasRestoreBounds() && !window_state->IsFloated()) {
      window_info->current_bounds = window_state->GetRestoreBoundsInScreen();
    } else {
      window_info->current_bounds =
          GetBoundsIgnoringTransforms(window, /*use_screen=*/true);
    }

    // Window restore does not support restoring fullscreen windows. If a window
    // is fullscreen save the pre-fullscreen window state instead.
    window_info->window_state_type =
        window_state->IsFullscreen()
            ? chromeos::ToWindowStateType(
                  window->GetProperty(aura::client::kRestoreShowStateKey))
            : window_state->GetStateType();
  }

  // Populate the restore show state field that the minimize should restore back
  // to if the window is minimized.
  if (window_state->IsMinimized()) {
    window_info->pre_minimized_show_state_type =
        window->GetProperty(aura::client::kRestoreShowStateKey);
  }

  if (window_state->IsSnapped()) {
    // `WindowState::snap_ratio_` is stored as a float between 0 and 1. Convert
    // it to a percentage here.
    std::optional<float> snap_ratio = window_state->snap_ratio();
    window_info->snap_percentage =
        snap_ratio.has_value() ? std::make_optional(std::round(
                                     100 * window_state->snap_ratio().value()))
                               : std::nullopt;
  }

  window_info->display_id =
      display::Screen::GetScreen()->GetDisplayNearestWindow(window).id();

  // For saved desks, store the readable app name so that we can have a nice
  // error message if the user tries to used the saved desk on a device that
  // doesn't have the app.
  std::string* app_id = window->GetProperty(kAppIDKey);
  window_info->app_title =
      app_id
          ? base::UTF8ToUTF16(
                Shell::Get()->saved_desk_delegate()->GetAppShortName(*app_id))
          : window->GetTitle();

  // Save window size restriction of ARC app window.
  if (IsArcWindow(window)) {
    views::Widget* widget = views::Widget::GetWidgetForNativeWindow(window);
    if (widget) {
      window_info->arc_extra_info = {
          .maximum_size = widget->GetMaximumSize(),
          .minimum_size = widget->GetMinimumSize(),
          .bounds_in_root =
              GetBoundsIgnoringTransforms(window, /*use_screen=*/false)};
      window_info->app_title = window->GetTitle();
    }
  }

  return window_info;
}

bool IsBrowserAppId(const std::string& id) {
  return id == app_constants::kChromeAppId || id == app_constants::kLacrosAppId;
}

base::FilePath GetInformedRestoreImagePath() {
  if (!informed_restore_image_path_for_test_.empty()) {
    return informed_restore_image_path_for_test_;
  }
  base::FilePath home_dir;
  CHECK(base::PathService::Get(base::DIR_HOME, &home_dir));
  return home_dir.AppendASCII("informed_restore_image.png");
}

void SetInformedRestoreImagePathForTest(const base::FilePath& path) {
  informed_restore_image_path_for_test_ = path;
}

}  // namespace ash
