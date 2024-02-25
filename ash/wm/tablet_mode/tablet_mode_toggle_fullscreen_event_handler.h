// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_TABLET_MODE_TABLET_MODE_TOGGLE_FULLSCREEN_EVENT_HANDLER_H_
#define ASH_WM_TABLET_MODE_TABLET_MODE_TOGGLE_FULLSCREEN_EVENT_HANDLER_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "ui/aura/window_observer.h"
#include "ui/events/event_handler.h"

namespace ui {
class TouchEvent;
}

namespace ash {

// TabletModeToggleFullscreenEventHandler handles toggling fullscreen when
// appropriate. TabletModeToggleFullscreenEventHandler installs event handlers
// in an environment specific way, e.g. EventHandler for aura.
class TabletModeToggleFullscreenEventHandler : public ui::EventHandler,
                                               public aura::WindowObserver {
 public:
  TabletModeToggleFullscreenEventHandler();
  TabletModeToggleFullscreenEventHandler(
      const TabletModeToggleFullscreenEventHandler&) = delete;
  TabletModeToggleFullscreenEventHandler& operator=(
      const TabletModeToggleFullscreenEventHandler&) = delete;
  ~TabletModeToggleFullscreenEventHandler() override;

 private:
  struct DragData {
    int start_y_location;
    raw_ptr<aura::Window> window;
  };

  // ui::EventHandler:
  void OnTouchEvent(ui::TouchEvent* event) override;

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;

  bool ProcessEvent(const ui::TouchEvent& event);

  // Returns true if |window| can be fullscreen toggled.
  bool CanToggleFullscreen(const aura::Window* window);

  // Resets |drag_data_| and remove the WindowObserver.
  void ResetDragData();

  // Valid if a processable drag is in progress. Contains the event initial
  // location and the window that was active when the drag started.
  std::optional<DragData> drag_data_;
};

}  // namespace ash

#endif  // ASH_WM_TABLET_MODE_TABLET_MODE_TOGGLE_FULLSCREEN_EVENT_HANDLER_H_
