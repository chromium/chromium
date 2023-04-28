// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_TABLET_MODE_TABLET_MODE_MULTITASK_MENU_EVENT_HANDLER_H_
#define ASH_WM_TABLET_MODE_TABLET_MODE_MULTITASK_MENU_EVENT_HANDLER_H_

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/aura/window_observer.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/geometry/rect_f.h"

namespace ash {

class TabletModeMultitaskMenu;
class TabletModeMultitaskCue;

// TabletModeMultitaskMenuEventHandler handles gestures in tablet mode that may
// show or hide the multitask menu.
class TabletModeMultitaskMenuEventHandler : public ui::EventHandler {
 public:
  TabletModeMultitaskMenuEventHandler();
  TabletModeMultitaskMenuEventHandler(
      const TabletModeMultitaskMenuEventHandler&) = delete;
  TabletModeMultitaskMenuEventHandler& operator=(
      const TabletModeMultitaskMenuEventHandler&) = delete;
  ~TabletModeMultitaskMenuEventHandler() override;

  static bool CanShowMenu(aura::Window* window);

  TabletModeMultitaskMenu* multitask_menu() { return multitask_menu_.get(); }
  TabletModeMultitaskCue* multitask_cue() { return multitask_cue_.get(); }

  // Creates and shows the menu.
  void ShowMultitaskMenu(aura::Window* window);

  // Destroys the multitask menu and resets the position of the cue if it is
  // visible.
  void ResetMultitaskMenu();

  // ui::EventHandler:
  void OnGestureEvent(ui::GestureEvent* event) override;

 private:
  void MaybeCreateMultitaskMenu(aura::Window* active_window);

  // True while a drag to open or close the menu is in progress. Needed since a
  // drag may go outside menu bounds, during which we still handle events.
  bool is_drag_active_ = false;

  // Creates a draggable bar when app windows are activated.
  std::unique_ptr<TabletModeMultitaskCue> multitask_cue_;

  std::unique_ptr<TabletModeMultitaskMenu> multitask_menu_;
};

}  // namespace ash

#endif  // ASH_WM_TABLET_MODE_TABLET_MODE_MULTITASK_MENU_EVENT_HANDLER_H_
