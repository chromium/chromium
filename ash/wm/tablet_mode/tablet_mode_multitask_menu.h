// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_TABLET_MODE_TABLET_MODE_MULTITASK_MENU_H_
#define ASH_WM_TABLET_MODE_TABLET_MODE_MULTITASK_MENU_H_

#include "ash/ash_export.h"
#include "ash/wm/tablet_mode/tablet_mode_multitask_menu_controller.h"
#include "base/memory/raw_ptr.h"
#include "ui/aura/window.h"
#include "ui/display/display_observer.h"
#include "ui/views/focus/widget_focus_manager.h"
#include "ui/views/widget/unique_widget_ptr.h"

namespace chromeos {
class MultitaskMenuView;
}

namespace ash {

class TabletModeMultitaskMenuController;
class TabletModeMultitaskMenuView;

// Creates and maintains the multitask menu. Responsible for showing,
// hiding, and animating the menu.
class ASH_EXPORT TabletModeMultitaskMenu
    : public views::WidgetFocusChangeListener,
      public display::DisplayObserver {
 public:
  TabletModeMultitaskMenu(TabletModeMultitaskMenuController* controller,
                          aura::Window* window);

  TabletModeMultitaskMenu(const TabletModeMultitaskMenu&) = delete;
  TabletModeMultitaskMenu& operator=(const TabletModeMultitaskMenu&) = delete;

  ~TabletModeMultitaskMenu() override;

  views::Widget* widget() { return widget_.get(); }

  // Performs a slide down animation on the menu (and cue if visible) if `show`
  // is true, otherwise a slide up animation.
  void Animate(bool show);

  // Performs a fade out animation and closes the menu.
  void AnimateFadeOut();

  // Actions called by the event handler, where `initial_y` and `current_y` are
  // in `window_`'s coordinates. If `down` is true, we are dragging down to show
  // the menu, else we are dragging up to hide the menu. Also makes the cue
  // follow the menu's movement if it is showing.
  void BeginDrag(float initial_y, bool down);
  void UpdateDrag(float current_y, bool down);
  void EndDrag();

  // Calls the event handler to destroy `this`.
  void Reset();

  // views::WidgetFocusChangeListener:
  void OnNativeFocusChanged(gfx::NativeView focused_now) override;

  // display::DisplayObserver:
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override;

  chromeos::MultitaskMenuView* GetMultitaskMenuViewForTesting();

 private:
  // The event handler that created this multitask menu. Guaranteed to outlive
  // `this`.
  raw_ptr<TabletModeMultitaskMenuController> controller_;

  // Widget implementation that is created and maintained by `this`.
  views::UniqueWidgetPtr widget_ = std::make_unique<views::Widget>();

  // The contents view of the above widget.
  raw_ptr<TabletModeMultitaskMenuView> menu_view_ = nullptr;

  // Initial y location in `window_` coordinates. Only relevant for drags.
  float initial_y_;

  display::ScopedOptionalDisplayObserver display_observer_{this};

  base::WeakPtrFactory<TabletModeMultitaskMenu> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_WM_TABLET_MODE_TABLET_MODE_MULTITASK_MENU_H_
