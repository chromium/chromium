// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_TABLET_MODE_TABLET_MODE_APP_WINDOW_DRAG_CONTROLLER_H_
#define ASH_WM_TABLET_MODE_TABLET_MODE_APP_WINDOW_DRAG_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/wm/wm_toplevel_window_event_handler.h"
#include "ui/display/display_observer.h"

namespace ui {
class GestureEvent;
}  // namespace ui

namespace ash {
class TabletModeWindowDragDelegate;

// Handles app windows dragging in tablet mode. App windows can be dragged into
// splitscreen through swiping from the top of the screen in tablet mode.
class ASH_EXPORT TabletModeAppWindowDragController
    : public display::DisplayObserver {
 public:
  TabletModeAppWindowDragController();
  ~TabletModeAppWindowDragController() override;

  // Processes a gesture event and updates the transform of |dragged_window_|.
  // Returns true if the gesture has been handled and it should not be processed
  // any further, false otherwise. Depending on the event position, the dragged
  // window may be 1) maximized, or 2) snapped in splitscren.
  bool DragWindowFromTop(ui::GestureEvent* event);

  TabletModeWindowDragDelegate* drag_delegate_for_testing() {
    return drag_delegate_.get();
  }

 private:
  // Gesture window drag related functions. Used in DragWindowFromTop.
  bool StartWindowDrag(ui::GestureEvent* event);
  void UpdateWindowDrag(ui::GestureEvent* event);
  void EndWindowDrag(ui::GestureEvent* event,
                     wm::WmToplevelWindowEventHandler::DragResult result);
  void FlingOrSwipe(ui::GestureEvent* event);

  // display::DisplayObserver:
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t metrics) override;

  std::unique_ptr<TabletModeWindowDragDelegate> drag_delegate_;
  gfx::Point previous_location_in_screen_;

  DISALLOW_COPY_AND_ASSIGN(TabletModeAppWindowDragController);
};

}  // namespace ash

#endif  // ASH_WM_TABLET_MODE_TABLET_MODE_APP_WINDOW_DRAG_CONTROLLER_H_
