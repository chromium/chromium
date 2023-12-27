// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_TABLET_MODE_TABLET_MODE_MULTITASK_MENU_CONTROLLER_H_
#define ASH_WM_TABLET_MODE_TABLET_MODE_MULTITASK_MENU_CONTROLLER_H_

#include <optional>

#include "ui/aura/window_observer.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/geometry/rect_f.h"

namespace ash {

class TabletModeMultitaskCueController;
class TabletModeMultitaskMenu;

// TabletModeMultitaskMenuController handles gestures in tablet mode that may
// show or hide the multitask menu.
class TabletModeMultitaskMenuController : public ui::EventHandler {
 public:
  TabletModeMultitaskMenuController();
  TabletModeMultitaskMenuController(const TabletModeMultitaskMenuController&) =
      delete;
  TabletModeMultitaskMenuController& operator=(
      const TabletModeMultitaskMenuController&) = delete;
  ~TabletModeMultitaskMenuController() override;

  static bool CanShowMenu(aura::Window* window);

  TabletModeMultitaskMenu* multitask_menu() { return multitask_menu_.get(); }
  TabletModeMultitaskCueController* multitask_cue_controller() {
    return multitask_cue_controller_.get();
  }

  // Creates and shows the menu.
  void ShowMultitaskMenu(aura::Window* window);

  // Destroys the multitask menu and resets the position of the cue if it is
  // visible.
  void ResetMultitaskMenu();

  // ui::EventHandler:
  void OnTouchEvent(ui::TouchEvent* event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

  // Accessors for testings.
  bool is_drag_active_for_test() const { return is_drag_active_; }

  bool reserved_for_gesture_sent_for_test() const {
    return reserved_for_gesture_sent_;
  }

  aura::Window* target_window_for_test() { return target_window_for_test_; }

 private:
  void MaybeCreateMultitaskMenu(aura::Window* active_window);

  // True while a drag to open or close the menu is in progress. Needed since a
  // drag may go outside menu bounds, during which we still handle events.
  bool is_drag_active_ = false;

  bool reserved_for_gesture_sent_ = false;

  // The target window that the menu was created on. Unused.
  raw_ptr<aura::Window, DanglingUntriaged> target_window_for_test_ = nullptr;

  // Creates a draggable bar when app windows are activated.
  std::unique_ptr<TabletModeMultitaskCueController> multitask_cue_controller_;

  std::unique_ptr<TabletModeMultitaskMenu> multitask_menu_;
};

}  // namespace ash

#endif  // ASH_WM_TABLET_MODE_TABLET_MODE_MULTITASK_MENU_CONTROLLER_H_
