// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_TABLET_MODE_TABLET_MODE_MULTITASK_MENU_H_
#define ASH_WM_TABLET_MODE_TABLET_MODE_MULTITASK_MENU_H_

#include "ash/ash_export.h"
#include "ash/wm/tablet_mode/tablet_mode_multitask_menu_event_handler.h"
#include "base/scoped_observation.h"
#include "ui/aura/window.h"
#include "ui/display/display_observer.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/widget/widget_observer.h"

namespace chromeos {
class MultitaskMenuView;
}

namespace ash {

class TabletModeMultitaskMenuEventHandler;

// The container of the multitask menu. Creates and owns the multitask menu
// widget.
class ASH_EXPORT TabletModeMultitaskMenu : aura::WindowObserver,
                                           public views::WidgetObserver,
                                           public display::DisplayObserver {
 public:
  TabletModeMultitaskMenu(TabletModeMultitaskMenuEventHandler* event_handler,
                          aura::Window* window,
                          base::RepeatingClosure hide_menu);

  TabletModeMultitaskMenu(const TabletModeMultitaskMenu&) = delete;
  TabletModeMultitaskMenu& operator=(const TabletModeMultitaskMenu&) = delete;

  ~TabletModeMultitaskMenu() override;

  aura::Window* window() { return window_; }

  views::Widget* multitask_menu_widget() {
    return multitask_menu_widget_.get();
  }

  // Show the menu using a slide down animation.
  void AnimateShow();

  // Close the menu using a slide up animation.
  void AnimateClose();

  // Calls the event handler to destroy `this`.
  void Reset();

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;

  // views::WidgetObserver:
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;

  // display::DisplayObserver:
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override;

  chromeos::MultitaskMenuView* GetMultitaskMenuViewForTesting();

 private:
  // The event handler that created this multitask menu. Guaranteed to outlive
  // `this`.
  TabletModeMultitaskMenuEventHandler* event_handler_;

  // The window associated with this multitask menu.
  aura::Window* window_ = nullptr;

  // Window observer for `window_`.
  base::ScopedObservation<aura::Window, aura::WindowObserver> observed_window_{
      this};

  base::ScopedObservation<views::Widget, views::WidgetObserver>
      widget_observation_{this};

  display::ScopedOptionalDisplayObserver display_observer_{this};

  views::UniqueWidgetPtr multitask_menu_widget_ =
      std::make_unique<views::Widget>();

  base::WeakPtrFactory<TabletModeMultitaskMenu> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_WM_TABLET_MODE_TABLET_MODE_MULTITASK_MENU_H_
