// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DRAG_WINDOW_CONTROLLER_H_
#define ASH_WM_DRAG_WINDOW_CONTROLLER_H_

#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "ui/gfx/geometry/rect.h"

namespace aura {
class Window;
}

namespace ui {
class LayerTreeOwner;
}

namespace ash {

// Handles visuals for window dragging as it relates to multi-display support.
// Phantom windows called "drag windows" represent the window on other displays.
class ASH_EXPORT DragWindowController {
 public:
  DragWindowController(aura::Window* window, bool is_touch_dragging);
  virtual ~DragWindowController();

  // Updates bounds and opacity for the drag windows, and creates/destroys each
  // drag window so that it only exists when it would have nonzero opacity. Also
  // updates the opacity of the original window.
  void Update();

 private:
  class DragWindowDetails;
  FRIEND_TEST_ALL_PREFIXES(DragWindowResizerTest, DragWindowController);
  FRIEND_TEST_ALL_PREFIXES(DragWindowResizerTest,
                           DragWindowControllerAcrossThreeDisplays);

  // Returns the currently active drag windows.
  int GetDragWindowsCountForTest() const;

  // Returns the drag window/layer owner for given index of the
  // currently active drag windows list.
  const aura::Window* GetDragWindowForTest(size_t index) const;
  const ui::LayerTreeOwner* GetDragLayerOwnerForTest(size_t index) const;

  // Call Layer::OnPaintLayer on all layers under the drag_windows_.
  void RequestLayerPaintForTest();

  // The original window.
  aura::Window* window_;

  // Indicates touch dragging, as opposed to mouse dragging.
  const bool is_touch_dragging_;

  // |window_|'s opacity before the drag. Used to revert opacity after the drag.
  const float old_opacity_;

  // Phantom windows to visually represent |window_| on other displays.
  std::vector<std::unique_ptr<DragWindowDetails>> drag_windows_;

  DISALLOW_COPY_AND_ASSIGN(DragWindowController);
};

}  // namespace ash

#endif  // ASH_WM_DRAG_WINDOW_CONTROLLER_H_
