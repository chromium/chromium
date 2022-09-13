// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_TABLET_MODE_TABLET_MODE_MULTITASK_MENU_EVENT_HANDLER_H_
#define ASH_WM_TABLET_MODE_TABLET_MODE_MULTITASK_MENU_EVENT_HANDLER_H_

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/aura/window_observer.h"
#include "ui/events/event_handler.h"

namespace ash {

class TabletModeMultitaskMenu;

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

  // ui::EventHandler:
  // TODO(crbug.com/1336836): Temporarily allow mouse wheel events to show or
  // hide the multitask menu for developers. Remove this before launch.
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

  void CloseMultitaskMenu();

  TabletModeMultitaskMenu* multitask_menu_for_testing() {
    return multitask_menu_.get();
  }

 private:
  bool ProcessBeginFlingOrSwipe(const ui::GestureEvent& event);

  void ShowMultitaskMenu(aura::Window* active_window);

  std::unique_ptr<TabletModeMultitaskMenu> multitask_menu_;

  // Used to show or hide the multitask menu. Null if no drag is in
  // progress.
  absl::optional<bool> is_drag_to_open_;
};

}  // namespace ash

#endif  // ASH_WM_TABLET_MODE_TABLET_MODE_MULTITASK_MENU_EVENT_HANDLER_H_
