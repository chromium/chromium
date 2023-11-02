// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/in_app_to_home_nudge_controller.h"

#include "ash/session/session_controller_impl.h"
#include "ash/shelf/drag_handle.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"

namespace ash {

InAppToHomeNudgeController::InAppToHomeNudgeController(
    ShelfWidget* shelf_widget)
    : shelf_widget_(shelf_widget) {}

InAppToHomeNudgeController::~InAppToHomeNudgeController() = default;

void InAppToHomeNudgeController::SetNudgeAllowedForCurrentShelf(
    bool in_tablet_mode,
    bool in_app_shelf,
    bool shelf_controls_visible) {
  // HideDragHandleNudge should hide the in app to home nudge if shelf controls
  // are enabled. We need the in_tablet_mode check to prevent misreporting the
  // hide cause when exiting tablet mode.
  if (shelf_controls_visible && in_tablet_mode) {
    shelf_widget_->HideDragHandleNudge(
        contextual_tooltip::DismissNudgeReason::kOther);
    return;
  }

  if (in_tablet_mode && in_app_shelf) {
    if (contextual_tooltip::ShouldShowNudge(
            Shell::Get()->session_controller()->GetLastActiveUserPrefService(),
            contextual_tooltip::TooltipType::kInAppToHome,
            /*recheck_delay*/ nullptr)) {
      shelf_widget_->ScheduleShowDragHandleNudge();
    } else if (!shelf_widget_->GetDragHandle()
                    ->gesture_nudge_target_visibility()) {
      // If the drag handle is not yet shown, HideDragHandleNudge() should
      // cancel any scheduled show requests.
      shelf_widget_->HideDragHandleNudge(
          contextual_tooltip::DismissNudgeReason::kOther);
    }
  } else {
    shelf_widget_->HideDragHandleNudge(
        in_tablet_mode
            ? contextual_tooltip::DismissNudgeReason::kExitToHomeScreen
            : contextual_tooltip::DismissNudgeReason::kSwitchToClamshell);
  }
}

}  // namespace ash
