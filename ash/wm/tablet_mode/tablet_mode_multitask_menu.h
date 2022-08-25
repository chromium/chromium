// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_TABLET_MODE_TABLET_MODE_MULTITASK_MENU_H_
#define ASH_WM_TABLET_MODE_TABLET_MODE_MULTITASK_MENU_H_

#include "ash/ash_export.h"
#include "ash/wm/tablet_mode/tablet_mode_multitask_menu_event_handler.h"
#include "base/scoped_observation.h"
#include "ui/aura/window.h"
#include "ui/views/widget/unique_widget_ptr.h"

namespace ash {

class TabletModeMultitaskMenuEventHandler;

// The container of the multitask menu. Creates and owns the multitask menu
// widget.
class ASH_EXPORT TabletModeMultitaskMenu : aura::WindowObserver {
 public:
  TabletModeMultitaskMenu(TabletModeMultitaskMenuEventHandler* event_handler,
                          aura::Window* window);

  TabletModeMultitaskMenu(const TabletModeMultitaskMenu&) = delete;
  TabletModeMultitaskMenu& operator=(const TabletModeMultitaskMenu&) = delete;

  ~TabletModeMultitaskMenu() override;

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;

  void Show();
  void Hide();

  aura::Window* window() { return window_; }

  views::Widget* multitask_menu_widget_for_testing() {
    return multitask_menu_widget_.get();
  }

 private:
  // The event handler that created this multitask menu. Guaranteed to outlive
  // `this`.
  TabletModeMultitaskMenuEventHandler* event_handler_;

  // The window associated with this multitask menu.
  aura::Window* window_ = nullptr;

  // Window observer for `window_`.
  base::ScopedObservation<aura::Window, aura::WindowObserver> observed_window_{
      this};

  views::UniqueWidgetPtr multitask_menu_widget_ =
      std::make_unique<views::Widget>();
};

}  // namespace ash

#endif  // ASH_WM_TABLET_MODE_TABLET_MODE_MULTITASK_MENU_H_