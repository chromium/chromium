// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desks_test_api.h"

#include "ash/shell.h"
#include "ash/system/toast/toast_manager_impl.h"
#include "ash/wm/desks/cros_next_desk_icon_button.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desk_action_context_menu.h"
#include "ash/wm/desks/desk_mini_view.h"
#include "ash/wm/desks/desk_preview_view.h"
#include "ash/wm/desks/desks_restore_util.h"
#include "ash/wm/desks/expanded_desks_bar_button.h"
#include "ash/wm/desks/legacy_desk_bar_view.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_test_util.h"
#include "ui/views/background.h"
#include "ui/views/controls/menu/menu_runner.h"

namespace ash {

namespace {

const LegacyDeskBarView* GetDesksBarView() {
  auto* root_window = Shell::GetPrimaryRootWindow();
  auto* overview_controller = Shell::Get()->overview_controller();
  DCHECK(overview_controller->InOverviewSession());
  return overview_controller->overview_session()
      ->GetGridWithRootWindow(root_window)
      ->desks_bar_view();
}

}  // namespace

// static
ScrollArrowButton* DesksTestApi::GetDesksBarLeftScrollButton() {
  return GetDesksBarView()->left_scroll_button_;
}

// static
ScrollArrowButton* DesksTestApi::GetDesksBarRightScrollButton() {
  return GetDesksBarView()->right_scroll_button_;
}

// static
views::ScrollView* DesksTestApi::GetDesksBarScrollView() {
  return GetDesksBarView()->scroll_view_;
}

// static
const DeskMiniView* DesksTestApi::GetDesksBarDragView() {
  return GetDesksBarView()->drag_view_;
}

// static
DeskActionContextMenu* DesksTestApi::GetContextMenuForDesk(int index) {
  return GetDesksBarView()->mini_views()[index]->context_menu_.get();
}

// static
views::LabelButton* DesksTestApi::GetCloseAllUndoToastDismissButton() {
  ToastManagerImpl* toast_manager = Shell::Get()->toast_manager();
  return toast_manager->GetCurrentOverlayForTesting()
      ->dismiss_button_for_testing();
}

// static
const ui::SimpleMenuModel& DesksTestApi::GetContextMenuModelForDesk(int index) {
  return GetContextMenuForDesk(index)->context_menu_model_;
}

// static
views::View* DesksTestApi::GetHighlightOverlayForDeskPreview(int index) {
  return GetDesksBarView()
      ->mini_views()[index]
      ->desk_preview()
      ->highlight_overlay_;
}

// static
ui::LayerTreeOwner* DesksTestApi::GetMirroredContentsLayerTreeForRootAndDesk(
    aura::Window* root,
    Desk* desk) {
  auto& mini_views = GetOverviewSession()
                         ->GetGridWithRootWindow(root)
                         ->desks_bar_view()
                         ->mini_views();
  for (auto* mini_view : mini_views) {
    if (mini_view->desk() == desk) {
      return mini_view->desk_preview()
          ->desk_mirrored_contents_layer_tree_owner_.get();
    }
  }
  return nullptr;
}

// static
bool DesksTestApi::DesksControllerHasDesk(Desk* desk) {
  return DesksController::Get()->HasDesk(desk);
}

// static
bool DesksTestApi::DesksControllerCanUndoDeskRemoval() {
  return DesksController::Get()->temporary_removed_desk_ != nullptr;
}

// static
bool DesksTestApi::IsContextMenuRunningForDesk(int index) {
  return GetContextMenuForDesk(index)->context_menu_runner_->IsRunning();
}

// static
bool DesksTestApi::IsDesksBarLeftGradientVisible() {
  const auto& gradient_mask =
      GetDesksBarView()->scroll_view_->layer()->gradient_mask();
  return !gradient_mask.IsEmpty() &&
         cc::MathUtil::IsWithinEpsilon(gradient_mask.steps()[0].fraction, 0.f);
}

// static
bool DesksTestApi::IsDesksBarRightGradientVisible() {
  const auto& gradient_mask =
      GetDesksBarView()->scroll_view_->layer()->gradient_mask();
  return !gradient_mask.IsEmpty() &&
         cc::MathUtil::IsWithinEpsilon(
             gradient_mask.steps()[gradient_mask.step_count() - 1].fraction,
             1.f);
}

// static
void DesksTestApi::ResetDeskVisitedMetrics(Desk* desk) {
  const int current_date = desks_restore_util::GetDaysFromLocalEpoch();
  desk->first_day_visited_ = current_date;
  desk->last_day_visited_ = current_date;
}

}  // namespace ash
