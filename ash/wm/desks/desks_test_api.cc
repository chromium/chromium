// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desks_test_api.h"

#include "ash/controls/gradient_layer_delegate.h"
#include "ash/shell.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desk_mini_view.h"
#include "ash/wm/desks/desks_bar_view.h"
#include "ash/wm/desks/desks_restore_util.h"
#include "ash/wm/desks/expanded_desks_bar_button.h"
#include "ash/wm/desks/persistent_desks_bar_button.h"
#include "ash/wm/desks/persistent_desks_bar_context_menu.h"
#include "ash/wm/desks/persistent_desks_bar_controller.h"
#include "ash/wm/desks/persistent_desks_bar_view.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"

namespace ash {

namespace {

const DesksBarView* GetDesksBarView() {
  auto* root_window = Shell::GetPrimaryRootWindow();
  auto* overview_controller = Shell::Get()->overview_controller();
  DCHECK(overview_controller->InOverviewSession());
  return overview_controller->overview_session()
      ->GetGridWithRootWindow(root_window)
      ->desks_bar_view();
}

const PersistentDesksBarView* GetPersistentDesksBarView() {
  auto* persistent_desks_bar_controller =
      Shell::Get()->persistent_desks_bar_controller();
  DCHECK(persistent_desks_bar_controller);
  return persistent_desks_bar_controller->persistent_desks_bar_view();
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
PersistentDesksBarContextMenu* DesksTestApi::GetDesksBarContextMenu() {
  return GetDesksBarView()->vertical_dots_button_->context_menu_.get();
}

// static
SkColor DesksTestApi::GetNewDeskButtonBackgroundColor() {
  return GetDesksBarView()
      ->expanded_state_new_desk_button()
      ->inner_button()
      ->background_color_;
}

// static
PersistentDesksBarContextMenu*
DesksTestApi::GetPersistentDesksBarContextMenu() {
  return GetPersistentDesksBarView()
      ->vertical_dots_button_->context_menu_.get();
}

// static
const std::vector<PersistentDesksBarDeskButton*>
DesksTestApi::GetPersistentDesksBarDeskButtons() {
  return GetPersistentDesksBarView()->desk_buttons_;
}

// static
DeskActionContextMenu* DesksTestApi::GetContextMenuForDesk(int index) {
  return GetDesksBarView()->mini_views()[index]->context_menu_.get();
}

// static
bool DesksTestApi::HasVerticalDotsButton() {
  return GetDesksBarView()->vertical_dots_button_;
}

// static
bool DesksTestApi::IsDesksBarLeftGradientVisible() {
  return !GetDesksBarView()
              ->gradient_layer_delegate_->start_fade_zone_bounds()
              .IsEmpty();
}

// static
bool DesksTestApi::IsDesksBarRightGradientVisible() {
  return !GetDesksBarView()
              ->gradient_layer_delegate_->end_fade_zone_bounds()
              .IsEmpty();
}

// static
void DesksTestApi::ResetDeskVisitedMetrics(Desk* desk) {
  const int current_date = desks_restore_util::GetDaysFromLocalEpoch();
  desk->first_day_visited_ = current_date;
  desk->last_day_visited_ = current_date;
}

}  // namespace ash
