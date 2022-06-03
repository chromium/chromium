// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/wm_shadow_controller_delegate.h"

#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_item.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/window_state.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/wm/core/shadow_types.h"

namespace ash {

WmShadowControllerDelegate::WmShadowControllerDelegate() = default;

WmShadowControllerDelegate::~WmShadowControllerDelegate() = default;

bool WmShadowControllerDelegate::ShouldShowShadowForWindow(
    const aura::Window* window) {
  // Hide the shadow if it is one of the splitscreen snapped windows.
  if (window->GetRootWindow() && RootWindowController::ForWindow(window)) {
    SplitViewController* split_view_controller =
        SplitViewController::Get(window);
    if (split_view_controller &&
        split_view_controller->IsWindowInSplitView(window)) {
      return false;
    }
  }

  // Hide the shadow while we are in overview mode.
  OverviewController* overview_controller = Shell::Get()->overview_controller();
  if (overview_controller && overview_controller->InOverviewSession()) {
    OverviewSession* overview_session = overview_controller->overview_session();
    // InOverviewSession() being true implies |overview_session| exists.
    DCHECK(overview_session);
    // Windows in overview that are not moving out of the active desk should not
    // have shadows.
    auto* overview_item = overview_session->GetOverviewItemForWindow(window);
    if (desks_util::BelongsToActiveDesk(const_cast<aura::Window*>(window)) &&
        overview_item && !overview_item->is_moving_to_another_desk()) {
      return false;
    }
  }

  // The shadow state will be updated when the window is added to a parent.
  if (!window->parent())
    return false;

  // Show the shadow if it's currently being dragged no matter of the window's
  // show state.
  auto* window_state = WindowState::Get(window);
  if (window_state && window_state->is_dragged())
    return ::wm::GetShadowElevationConvertDefault(window) > 0;

  // Hide the shadow if it's not being dragged and it's a maximized/fullscreen
  // window.
  ui::WindowShowState show_state =
      window->GetProperty(aura::client::kShowStateKey);
  if (show_state == ui::SHOW_STATE_FULLSCREEN ||
      show_state == ui::SHOW_STATE_MAXIMIZED) {
    return false;
  }

  return ::wm::GetShadowElevationConvertDefault(window) > 0;
}

}  // namespace ash
