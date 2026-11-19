// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_TABLET_MODE_TABLET_MODE_MULTITASK_MENU_CONTROLLER_H_
#define ASH_WM_TABLET_MODE_TABLET_MODE_MULTITASK_MENU_CONTROLLER_H_

#include <memory>
#include <optional>

#include "base/memory/weak_ptr.h"
#include "ui/aura/window_observer.h"
#include "ui/events/event_handler.h"

namespace views {
class Widget;
}  // namespace views

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

  // Closes the multitask menu `widget`, which the menu is giving up as it is
  // destroyed, and deletes it in a later task.
  void CloseMultitaskMenuWidget(std::unique_ptr<views::Widget> widget);

  // ui::EventHandler:
  void OnTouchEvent(ui::TouchEvent* event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

  // Accessors for testings.
  bool is_drag_active_for_test() const { return is_drag_active_; }

  bool reserved_for_gesture_sent_for_test() const {
    return reserved_for_gesture_sent_;
  }

 private:
  void MaybeCreateMultitaskMenu(aura::Window* active_window);

  void BeginDrag(ui::GestureEvent* event, int y, bool down);

  // Deletes `closed_multitask_menu_widget_`.
  void DeleteClosedMultitaskMenuWidget();

  // True while a drag to open or close the menu is in progress. Needed since a
  // drag may go outside menu bounds, during which we still handle events.
  bool is_drag_active_ = false;

  bool reserved_for_gesture_sent_ = false;

  // Creates a draggable bar when app windows are activated.
  std::unique_ptr<TabletModeMultitaskCueController> multitask_cue_controller_;

  // The multitask menu widget after the menu closed it, kept until it can be
  // deleted outside of its own event handling.
  std::unique_ptr<views::Widget> closed_multitask_menu_widget_;

  std::unique_ptr<TabletModeMultitaskMenu> multitask_menu_;

  base::WeakPtrFactory<TabletModeMultitaskMenuController> weak_ptr_factory_{
      this};
};

}  // namespace ash

#endif  // ASH_WM_TABLET_MODE_TABLET_MODE_MULTITASK_MENU_CONTROLLER_H_
