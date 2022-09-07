// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/app_list_keyboard_controller.h"

#include <algorithm>

#include "ash/app_list/app_list_view_provider.h"
#include "ash/app_list/views/app_list_item_view.h"
#include "ash/app_list/views/app_list_toast_container_view.h"
#include "ash/app_list/views/apps_grid_view.h"
#include "ash/app_list/views/recent_apps_view.h"
#include "base/check.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view.h"

namespace ash {

AppListKeyboardController::AppListKeyboardController(
    AppListViewProvider* view_provider)
    : view_provider_(view_provider) {
  DCHECK(view_provider_);
}

void AppListKeyboardController::MoveFocusDownFromRecents(int column) {
  // Check if the toast container can handle the focus.
  auto* toast_container = view_provider_->GetToastContainerView();
  if (toast_container && toast_container->HandleFocus(column))
    return;

  HandleMovingFocusToAppsGrid(column);
}

void AppListKeyboardController::MoveFocusUpFromRecents() {
  RecentAppsView* recent_apps = view_provider_->GetRecentAppsView();
  DCHECK(recent_apps);
  DCHECK_GT(recent_apps->GetItemViewCount(), 0);

  AppListItemView* first_recent = recent_apps->GetItemViewAt(0);
  // Find the view one step in reverse from the first recent app.
  views::View* previous_view =
      recent_apps->GetFocusManager()->GetNextFocusableView(
          first_recent, recent_apps->GetWidget(), /*reverse=*/true,
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
  // Check if the toast container can handle the focus.
  auto* toast_container = view_provider_->GetToastContainerView();
  if (toast_container && toast_container->HandleFocus(column))
    return true;

  return HandleMovingFocusToRecents(column);
}

bool AppListKeyboardController::HandleMovingFocusToAppsGrid(int column) {
  AppsGridView* apps_grid_view = view_provider_->GetAppsGridView();
  DCHECK(apps_grid_view);
  int top_level_item_count = apps_grid_view->view_model()->view_size();
  if (top_level_item_count <= 0)
    return false;

  // Attempt to focus the item at `column` in the first row, or the last
  // item if there aren't enough items. This could happen if the user's apps
  // are in a small number of folders.
  int index = std::min(column, top_level_item_count - 1);
  AppListItemView* item = apps_grid_view->GetItemViewAt(index);
  DCHECK(item);
  item->RequestFocus();
  return true;
}

bool AppListKeyboardController::HandleMovingFocusToRecents(int column) {
  RecentAppsView* recent_apps = view_provider_->GetRecentAppsView();
  // If there aren't any recent apps, don't change focus here. Fall back to the
  // app grid's default behavior.
  if (!recent_apps || !recent_apps->GetVisible() ||
      recent_apps->GetItemViewCount() <= 0) {
    return false;
  }

  // Attempt to focus the item at `column`, or the last item if there aren't
  // enough items.
  int index = std::min(column, recent_apps->GetItemViewCount() - 1);
  AppListItemView* item = recent_apps->GetItemViewAt(index);
  DCHECK(item);
  item->RequestFocus();
  return true;
}

}  // namespace ash
