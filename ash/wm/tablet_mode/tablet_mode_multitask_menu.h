// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_TABLET_MODE_TABLET_MODE_MULTITASK_MENU_H_
#define ASH_WM_TABLET_MODE_TABLET_MODE_MULTITASK_MENU_H_

#include "ash/ash_export.h"
#include "ui/views/widget/unique_widget_ptr.h"

namespace ash {

// The container of the multitask menu. Creates and owns the multitask menu
// widget.
class ASH_EXPORT TabletModeMultitaskMenu {
 public:
  explicit TabletModeMultitaskMenu(aura::Window* window);

  TabletModeMultitaskMenu(const TabletModeMultitaskMenu&) = delete;
  TabletModeMultitaskMenu& operator=(const TabletModeMultitaskMenu&) = delete;

  ~TabletModeMultitaskMenu();

  void Show();
  void Hide();

  aura::Window* window() { return window_; }

  views::Widget* multitask_menu_widget_for_testing() {
    return multitask_menu_widget_.get();
  }

 private:
  // The window associated with this multitask menu.
  aura::Window* const window_;

  views::UniqueWidgetPtr multitask_menu_widget_ =
      std::make_unique<views::Widget>();
};

}  // namespace ash

#endif  // ASH_WM_TABLET_MODE_TABLET_MODE_MULTITASK_MENU_H_