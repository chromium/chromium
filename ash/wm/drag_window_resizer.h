// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DRAG_WINDOW_RESIZER_H_
#define ASH_WM_DRAG_WINDOW_RESIZER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/wm/window_resizer.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "ui/gfx/geometry/point_f.h"

namespace ash {

class DragWindowController;

// DragWindowResizer is a decorator of WindowResizer and adds the ability to
// drag windows across displays.
class ASH_EXPORT DragWindowResizer : public WindowResizer {
 public:
  // Creates DragWindowResizer that adds the ability of dragging windows across
  // displays to |next_window_resizer|.
  DragWindowResizer(std::unique_ptr<WindowResizer> next_window_resizer,
                    WindowState* window_state);

  DragWindowResizer(const DragWindowResizer&) = delete;
  DragWindowResizer& operator=(const DragWindowResizer&) = delete;

  ~DragWindowResizer() override;

  // WindowResizer:
  void Drag(const gfx::PointF& location, int event_flags) override;
  void CompleteDrag() override;
  void RevertDrag() override;
  void FlingOrSwipe(ui::GestureEvent* event) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(DragWindowResizerTest, DragWindowController);
  FRIEND_TEST_ALL_PREFIXES(DragWindowResizerTest,
                           DragWindowControllerLatchesTargetOpacity);
  FRIEND_TEST_ALL_PREFIXES(DragWindowResizerTest,
                           DragWindowControllerAcrossThreeDisplays);

  // Updates the bounds of the drag window for window dragging.
  void UpdateDragWindow();

  // Returns true if we should allow the mouse pointer to warp.
  bool ShouldAllowMouseWarp();

  // Do the real work of ending a drag. If the drag ends in a different display,
  // move the dragged window to that display.
  void EndDragImpl();

  std::unique_ptr<WindowResizer> next_window_resizer_;

  // Shows a semi-transparent image of the window being dragged.
  std::unique_ptr<DragWindowController> drag_window_controller_;

  gfx::PointF last_mouse_location_;

  // Current instance for use by the DragWindowResizerTest.
  static DragWindowResizer* instance_;

  base::WeakPtrFactory<DragWindowResizer> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_WM_DRAG_WINDOW_RESIZER_H_
