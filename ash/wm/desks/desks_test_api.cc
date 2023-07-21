// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desks_test_api.h"

#include "ash/shell.h"
#include "ash/system/toast/toast_manager_impl.h"
#include "ash/wm/desks/cros_next_desk_icon_button.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desk_action_context_menu.h"
#include "ash/wm/desks/desk_bar_controller.h"
#include "ash/wm/desks/desk_bar_view_base.h"
#include "ash/wm/desks/desk_mini_view.h"
#include "ash/wm/desks/desk_preview_view.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_restore_util.h"
#include "ash/wm/desks/expanded_desks_bar_button.h"
#include "ash/wm/desks/legacy_desk_bar_view.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_test_util.h"
#include "ui/compositor/layer.h"
#include "ui/views/background.h"
#include "ui/views/controls/menu/menu_runner.h"

namespace ash {

namespace {

const DeskBarViewBase* GetDeskBarView(DeskBarViewBase::Type type) {
  auto* root_window = Shell::GetPrimaryRootWindow();
  if (type == DeskBarViewBase::Type::kOverview) {
    auto* overview_controller = Shell::Get()->overview_controller();
    CHECK(overview_controller->InOverviewSession());
    return overview_controller->overview_session()
        ->GetGridWithRootWindow(root_window)
        ->desks_bar_view();
  } else {
    auto* desk_bar_controller = DesksController::Get()->desk_bar_controller();
    CHECK(desk_bar_controller);
    return desk_bar_controller->GetDeskBarView(root_window);
  }
}

}  // namespace

// static
ScrollArrowButton* DesksTestApi::GetDeskBarLeftScrollButton(
    DeskBarViewBase::Type type) {
  return GetDeskBarView(type)->left_scroll_button_;
}

// static
ScrollArrowButton* DesksTestApi::GetDeskBarRightScrollButton(
    DeskBarViewBase::Type type) {
  return GetDeskBarView(type)->right_scroll_button_;
}

// static
views::ScrollView* DesksTestApi::GetDeskBarScrollView(
    DeskBarViewBase::Type type) {
  return GetDeskBarView(type)->scroll_view_;
}

// static
const DeskMiniView* DesksTestApi::GetDeskBarDragView(
    DeskBarViewBase::Type type) {
  return GetDeskBarView(type)->drag_view_;
}

// static
DeskActionContextMenu* DesksTestApi::GetContextMenuForDesk(
    DeskBarViewBase::Type type,
    int index) {
  return GetDeskBarView(type)->mini_views()[index]->context_menu_.get();
}

// static
views::LabelButton* DesksTestApi::GetCloseAllUndoToastDismissButton() {
  ToastManagerImpl* toast_manager = Shell::Get()->toast_manager();
  return toast_manager->GetCurrentOverlayForTesting()
      ->dismiss_button_for_testing();
}

// static
const ui::SimpleMenuModel& DesksTestApi::GetContextMenuModelForDesk(
    DeskBarViewBase::Type type,
    int index) {
  return GetContextMenuForDesk(type, index)->context_menu_model_;
}

// static
views::View* DesksTestApi::GetHighlightOverlayForDeskPreview(
    DeskBarViewBase::Type type,
    int index) {
  return GetDeskBarView(type)
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
bool DesksTestApi::IsDeskShortcutViewVisible(DeskMiniView* mini_view) {
  // If the mini_view is in the overview desk bar desk_shortcut_view_ will be
  // nullptr.
  return mini_view->desk_shortcut_view_ &&
         mini_view->desk_shortcut_view_->GetVisible();
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
bool DesksTestApi::IsContextMenuRunningForDesk(DeskBarViewBase::Type type,
                                               int index) {
  return GetContextMenuForDesk(type, index)->context_menu_runner_->IsRunning();
}

// static
bool DesksTestApi::IsDeskBarLeftGradientVisible(DeskBarViewBase::Type type) {
  const auto& gradient_mask =
      GetDeskBarView(type)->scroll_view_->layer()->gradient_mask();
  return !gradient_mask.IsEmpty() &&
         cc::MathUtil::IsWithinEpsilon(gradient_mask.steps()[0].fraction, 0.f);
}

// static
bool DesksTestApi::IsDeskBarRightGradientVisible(DeskBarViewBase::Type type) {
  const auto& gradient_mask =
      GetDeskBarView(type)->scroll_view_->layer()->gradient_mask();
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

// static
void DesksTestApi::WaitForDeskBarUiUpdate(DeskBarViewBase* desk_bar_view) {
  base::RunLoop run_loop;
  desk_bar_view->on_update_ui_closure_for_testing_ = run_loop.QuitClosure();
  run_loop.Run();
}

}  // namespace ash
