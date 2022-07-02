// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_APP_LIST_KEYBOARD_CONTROLLER_H_
#define ASH_APP_LIST_VIEWS_APP_LIST_KEYBOARD_CONTROLLER_H_

#include "ash/ash_export.h"

namespace views {
class View;
}  // namespace views

namespace ash {

class AppListToastContainerView;
class AppsGridView;
class RecentAppsView;

// Common code that implements keyboard traversal logic in
// `AppListBubbleAppsPage` and `AppsContainerView`.
class ASH_EXPORT AppListKeyboardController {
 public:
  AppListKeyboardController(views::View* app_list_view,
                            RecentAppsView* recent_apps,
                            AppListToastContainerView* toast_container,
                            AppsGridView* apps_grid_view);
  AppListKeyboardController(const AppListKeyboardController&) = delete;
  AppListKeyboardController& operator=(const AppListKeyboardController&) =
      delete;
  ~AppListKeyboardController() = default;

  // Moves focus down/up and out (usually to the apps grid / continue tasks).
  // `column` is the column of the items that was focused in the recent apps
  // list.
  void MoveFocusDownFromRecents(int column);
  void MoveFocusUpFromRecents();

  // Attempts to move focus down/up and out (usually to the apps grid / recent
  // apps).
  // `column` is the column of the item that was focused before moving
  // focus on this toast container.
  bool MoveFocusDownFromToast(int column);
  bool MoveFocusUpFromToast(int column);

  // Attempts to move focus up and out (usually to the recent apps list).
  // `column` is the column of the item that was focused in the grid.
  bool MoveFocusUpFromAppsGrid(int column);

 private:
  // Helper functions to move the focus to RecentAppsView/AppsGridView.
  bool HandleMovingFocusToAppsGrid(int column);
  bool HandleMovingFocusToRecents(int column);

  views::View* const app_list_view_;
  RecentAppsView* const recent_apps_;
  AppListToastContainerView* const toast_container_;
  AppsGridView* const apps_grid_view_;
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_APP_LIST_KEYBOARD_CONTROLLER_H_
