// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/apps_grid_view_test_api.h"

#include <memory>
#include <vector>

#include "ash/app_list/views/app_list_item_view.h"
#include "ash/app_list/views/apps_grid_view.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "ui/aura/window.h"
#include "ui/compositor/test/layer_animation_stopped_waiter.h"
#include "ui/events/event.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace test {

AppsGridViewTestApi::AppsGridViewTestApi(AppsGridView* view) : view_(view) {}

AppsGridViewTestApi::~AppsGridViewTestApi() = default;

views::View* AppsGridViewTestApi::GetViewAtModelIndex(int index) const {
  return view_->view_model_.view_at(index);
}

void AppsGridViewTestApi::LayoutToIdealBounds() {
  if (view_->reorder_timer_.IsRunning()) {
    view_->reorder_timer_.Stop();
    view_->OnReorderTimer();
  }
  view_->DeprecatedLayoutImmediately();
}

gfx::Rect AppsGridViewTestApi::GetItemTileRectOnCurrentPageAt(int row,
                                                              int col) const {
  int slot = row * (view_->cols()) + col;
  gfx::Rect bounds_in_ltr =
      view_->GetExpectedTileBounds(GridIndex(view_->GetSelectedPage(), slot));
  // `GetExpectedTileBounds()` returns expected bounds for item at provided grid
  // index in LTR UI. Make sure this method returns mirrored bounds in RTL UI.
  return view_->GetMirroredRect(bounds_in_ltr);
}

void AppsGridViewTestApi::PressItemAt(int index) {
  GetViewAtModelIndex(index)->OnKeyPressed(
      ui::KeyEvent(ui::EventType::kKeyPressed, ui::VKEY_RETURN, ui::EF_NONE));
}

size_t AppsGridViewTestApi::TilesPerPageInPagedGrid(int page) const {
  return *view_->TilesPerPage(page);
}

size_t AppsGridViewTestApi::TilesPerPageOr(int page,
                                           size_t default_value) const {
  return view_->TilesPerPage(page).value_or(default_value);
}

int AppsGridViewTestApi::AppsOnPage(int page) const {
  return view_->GetNumberOfItemsOnPage(page);
}

AppListItemView* AppsGridViewTestApi::GetViewAtIndex(GridIndex index) const {
  return view_->GetViewAtIndex(index);
}

AppListItemView* AppsGridViewTestApi::GetViewAtVisualIndex(int page,
                                                           int slot) const {
  return GetViewAtIndex(GridIndex(page, slot));
}

const std::string& AppsGridViewTestApi::GetNameAtVisualIndex(int page,
                                                             int slot) const {
  return GetViewAtVisualIndex(page, slot)->item()->name();
}

gfx::Rect AppsGridViewTestApi::GetItemTileRectAtVisualIndex(int page,
                                                            int slot) const {
  // `GetExpectedTileBounds()` returns expected bounds for item at provided grid
  // index in LTR UI. Make sure this method returns mirrored bounds in RTL UI.
  return view_->GetMirroredRect(
      view_->GetExpectedTileBounds(GridIndex(page, slot)));
}

void AppsGridViewTestApi::WaitForItemMoveAnimationDone() {
  ui::LayerAnimationStoppedWaiter animation_waiter;
  while (true) {
    bool found_animation = false;
    for (size_t i = 0; i < view_->view_model()->view_size(); i++) {
      auto* item_view = view_->view_model()->view_at(i);
      if (view_->IsAnimatingView(item_view)) {
        found_animation = true;
        animation_waiter.Wait(item_view->layer());
        break;
      }
    }

    if (!found_animation)
      break;
  }
}

void AppsGridViewTestApi::FireReorderTimerAndWaitForAnimationDone() {
  base::OneShotTimer* timer = &view_->reorder_timer_;
  if (timer->IsRunning())
    timer->FireNow();

  WaitForItemMoveAnimationDone();
}

void AppsGridViewTestApi::ReorderItemByDragAndDrop(int source_index,
                                                   int target_index) {
  if (source_index == target_index)
    return;

  ui::test::EventGenerator event_generator(
      view_->GetWidget()->GetNativeView()->GetRootWindow());
  ash::AppListItemView* dragged_view =
      view_->view_model()->view_at(source_index);
  event_generator.MoveMouseTo(dragged_view->GetBoundsInScreen().CenterPoint());
  event_generator.PressLeftButton();
  dragged_view->FireMouseDragTimerForTest();

  // Calculate the move target location. If `source_index` is to the left of
  // `target_index`, the item should be moved to the right of the target slot
  // in order to trigger apps reorder; otherwise, the item should be moved to
  // the left.
  const gfx::Rect target_view_screen_bounds =
      view_->view_model()->view_at(target_index)->GetBoundsInScreen();
  constexpr int offset = 10;
  const int target_location_x =
      (source_index < target_index ? target_view_screen_bounds.right() + offset
                                   : target_view_screen_bounds.x() - offset);
  const gfx::Point target_move_location(
      target_location_x, target_view_screen_bounds.CenterPoint().y());
  event_generator.MoveMouseTo(target_move_location);
  FireReorderTimerAndWaitForAnimationDone();
  event_generator.ReleaseLeftButton();
}

}  // namespace test
}  // namespace ash
