// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_TABLET_MODE_TABLET_MODE_MULTITASK_MENU_EVENT_HANDLER_H_
#define ASH_WM_TABLET_MODE_TABLET_MODE_MULTITASK_MENU_EVENT_HANDLER_H_

#include "ui/aura/window_observer.h"
#include "ui/events/event_handler.h"

namespace ash {

class TabletModeMultitaskMenu;

// TabletModeMultitaskMenuEventHandler handles gestures in tablet mode that may
// show or hide the multitask menu.
class TabletModeMultitaskMenuEventHandler : public ui::EventHandler,
                                            public aura::WindowObserver {
 public:
  TabletModeMultitaskMenuEventHandler();
  TabletModeMultitaskMenuEventHandler(
      const TabletModeMultitaskMenuEventHandler&) = delete;
  TabletModeMultitaskMenuEventHandler& operator=(
      const TabletModeMultitaskMenuEventHandler&) = delete;
  ~TabletModeMultitaskMenuEventHandler() override;

  // ui::EventHandler:
  void OnGestureEvent(ui::GestureEvent* event) override;

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;

  TabletModeMultitaskMenu* multitask_menu_for_testing() {
    return multitask_menu_.get();
  }

 private:
  // TODO(crbug.com/1349534): Override touch events on windows underneath.

  void ShowMultitaskMenu(aura::Window* active_window);
  void HideMultitaskMenu();

  std::unique_ptr<TabletModeMultitaskMenu> multitask_menu_;

  // True if a swipe down gesture that can trigger the multitask menu was
  // started.
  bool swipe_down_started_ = false;
};

}  // namespace ash

#endif  // ASH_WM_TABLET_MODE_TABLET_MODE_MULTITASK_MENU_EVENT_HANDLER_H_