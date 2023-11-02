// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_TABLET_MODE_TABLET_MODE_WINDOW_RESIZER_H_
#define ASH_WM_TABLET_MODE_TABLET_MODE_WINDOW_RESIZER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/wm/window_resizer.h"
#include "base/memory/weak_ptr.h"
#include "ui/gfx/geometry/point_f.h"

namespace ash {

class TabletModeWindowDragDelegate;
class WindowState;

// WindowResizer implementation for windows in tablet mode. Currently we
// don't allow any resizing and any dragging happening on the area other than
// the caption tabs area in tablet mode, or the top few client pixels for app
// windows without caption areas. Depending on the event position, the dragged
// window may be 1) maximized, or 2) snapped in splitscreen, or 3) merged to an
// existing window (in the case of a browser window).
class ASH_EXPORT TabletModeWindowResizer : public WindowResizer {
 public:
  TabletModeWindowResizer(
      WindowState* window_state,
      std::unique_ptr<TabletModeWindowDragDelegate> drag_delegate);

  TabletModeWindowResizer(const TabletModeWindowResizer&) = delete;
  TabletModeWindowResizer& operator=(const TabletModeWindowResizer&) = delete;

  ~TabletModeWindowResizer() override;

  // WindowResizer:
  void Drag(const gfx::PointF& location_in_parent, int event_flags) override;
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

  gfx::PointF previous_location_in_screen_;

  bool did_lock_cursor_ = false;

  // Used to determine if this has been deleted during a drag such as when a tab
  // gets dragged into another browser window.
  base::WeakPtrFactory<TabletModeWindowResizer> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_WM_TABLET_MODE_TABLET_MODE_WINDOW_RESIZER_H_
