// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_TABLET_MODE_TABLET_MODE_BROWSER_WINDOW_DRAG_CONTROLLER_H_
#define ASH_WM_TABLET_MODE_TABLET_MODE_BROWSER_WINDOW_DRAG_CONTROLLER_H_

#include <memory>

#include "ash/wm/window_resizer.h"
#include "ash/wm/wm_toplevel_window_event_handler.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"

namespace ash {
class TabletModeWindowDragDelegate;

namespace wm {
class WindowState;
}  // namespace wm

// WindowResizer implementation for browser windows in tablet mode. Currently we
// don't allow any resizing and any dragging happening on the area other than
// the caption tabs area in tablet mode. Only browser windows with tabs are
// allowed to be dragged. Depending on the event position, the dragged window
// may be 1) maximized, or 2) snapped in splitscreen, or 3) merged to an
// existing window.
class TabletModeBrowserWindowDragController : public WindowResizer {
 public:
  explicit TabletModeBrowserWindowDragController(wm::WindowState* window_state);
  ~TabletModeBrowserWindowDragController() override;

  // WindowResizer:
  void Drag(const gfx::Point& location_in_parent, int event_flags) override;
  void CompleteDrag() override;
  void RevertDrag() override;
  void FlingOrSwipe(ui::GestureEvent* event) override;

  TabletModeWindowDragDelegate* drag_delegate_for_testing() {
    return drag_delegate_.get();
  }

 private:
  // The drag delegate that does the real work: shows/hides the drag indicators,
  // preview windows, blurred background, etc, during dragging.
  std::unique_ptr<TabletModeWindowDragDelegate> drag_delegate_;

  gfx::Point previous_location_in_screen_;

  bool did_lock_cursor_ = false;

  // Used to determine if this has been deleted during a drag such as when a tab
  // gets dragged into another browser window.
  base::WeakPtrFactory<TabletModeBrowserWindowDragController> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(TabletModeBrowserWindowDragController);
};

}  // namespace ash

#endif  // ASH_WM_TABLET_MODE_TABLET_MODE_BROWSER_WINDOW_DRAG_CONTROLLER_H_
