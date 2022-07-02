// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/app_list_keyboard_controller.h"

#include <algorithm>

#include "ash/app_list/views/app_list_item_view.h"
#include "ash/app_list/views/app_list_toast_container_view.h"
#include "ash/app_list/views/apps_grid_view.h"
#include "ash/app_list/views/recent_apps_view.h"
#include "base/check.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view.h"

namespace ash {

AppListKeyboardController::AppListKeyboardController(
    views::View* app_list_view,
    RecentAppsView* recent_apps,
    AppListToastContainerView* toast_container,
    AppsGridView* apps_grid_view)
    : app_list_view_(app_list_view),
      recent_apps_(recent_apps),
      toast_container_(toast_container),
      apps_grid_view_(apps_grid_view) {}

void AppListKeyboardController::MoveFocusDownFromRecents(int column) {
  // Check if the `toast_container_` can handle the focus.
  if (toast_container_ && toast_container_->HandleFocus(column))
    return;

  HandleMovingFocusToAppsGrid(column);
}

void AppListKeyboardController::MoveFocusUpFromRecents() {
  DCHECK(app_list_view_);
  DCHECK(recent_apps_);
  DCHECK_GT(recent_apps_->GetItemViewCount(), 0);

  AppListItemView* first_recent = recent_apps_->GetItemViewAt(0);
  // Find the view one step in reverse from the first recent app.
  views::View* previous_view =
      app_list_view_->GetFocusManager()->GetNextFocusableView(
          first_recent, app_list_view_->GetWidget(), /*reverse=*/true,
          /*dont_loop=*/false);
  DCHECK(previous_view);
  previous_view->RequestFocus();
}

bool AppListKeyboardController::MoveFocusDownFromToast(int column) {
  return HandleMovingFocusToAppsGrid(column);
}

bool AppListKeyboardController::MoveFocusUpFromToast(int column) {
  return HandleMovingFocusToRecents(column);
}

bool AppListKeyboardController::MoveFocusUpFromAppsGrid(int column) {
  // Check if the `toast_container_` can handle the focus.
  if (toast_container_ && toast_container_->HandleFocus(column))
    return true;

  return HandleMovingFocusToRecents(column);
}

bool AppListKeyboardController::HandleMovingFocusToAppsGrid(int column) {
  DCHECK(apps_grid_view_);
  int top_level_item_count = apps_grid_view_->view_model()->view_size();
  if (top_level_item_count <= 0)
    return false;

  // Attempt to focus the item at `column` in the first row, or the last
  // item if there aren't enough items. This could happen if the user's apps
  // are in a small number of folders.
  int index = std::min(column, top_level_item_count - 1);
  AppListItemView* item = apps_grid_view_->GetItemViewAt(index);
  DCHECK(item);
  item->RequestFocus();
  return true;
}

bool AppListKeyboardController::HandleMovingFocusToRecents(int column) {
  // If there aren't any recent apps, don't change focus here. Fall back to the
  // app grid's default behavior.
  if (!recent_apps_ || !recent_apps_->GetVisible() ||
      recent_apps_->GetItemViewCount() <= 0)
    return false;

  // Attempt to focus the item at `column`, or the last item if there aren't
  // enough items.
  int index = std::min(column, recent_apps_->GetItemViewCount() - 1);
  AppListItemView* item = recent_apps_->GetItemViewAt(index);
  DCHECK(item);
  item->RequestFocus();
  return true;
}

}  // namespace ash
