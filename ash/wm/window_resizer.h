// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_RESIZER_H_
#define ASH_WM_WINDOW_RESIZER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/public/cpp/presentation_time_recorder.h"
#include "ash/wm/drag_details.h"
#include "ash/wm/window_state.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "ui/wm/public/window_move_client.h"

namespace aura {
class Window;
}

namespace gfx {
class Rect;
}

namespace ui {
class GestureEvent;
}

namespace ash {

// WindowResizer is used by ToplevelWindowEventFilter to handle dragging, moving
// or resizing a window. All coordinates passed to this are in the parent
// windows coordinates.
class ASH_EXPORT WindowResizer {
 public:
  // Constants to identify the type of resize.
  static const int kBoundsChange_None;
  static const int kBoundsChange_Repositions;
  static const int kBoundsChange_Resizes;

  // Used to indicate which direction the resize occurs in.
  static const int kBoundsChangeDirection_None;
  static const int kBoundsChangeDirection_Horizontal;
  static const int kBoundsChangeDirection_Vertical;

  explicit WindowResizer(WindowState* window_state);
  virtual ~WindowResizer();

  // Returns a bitmask of the kBoundsChange_ values.
  static int GetBoundsChangeForWindowComponent(int component);

  // Returns a bitmask of the kBoundsChange_ values.
  static int GetPositionChangeDirectionForWindowComponent(int window_component);

  // Invoked to drag/move/resize the window. |location| is in the coordinates
  // of the window supplied to the constructor. |event_flags| is the event
  // flags from the event.
  virtual void Drag(const gfx::Point& location, int event_flags) = 0;

  // Invoked to complete the drag.
  virtual void CompleteDrag() = 0;

  // Reverts the drag.
  virtual void RevertDrag() = 0;

  // Flings or Swipes to end the drag.
  virtual void FlingOrSwipe(ui::GestureEvent* event) = 0;

  // Returns the target window the resizer was created for.
  aura::Window* GetTarget() const;

  // See comment for |DragDetails::initial_location_in_parent|.
  const gfx::Point& GetInitialLocation() const {
    return window_state_->drag_details()->initial_location_in_parent;
  }

  // Drag parameters established when drag starts.
  const DragDetails& details() const { return *window_state_->drag_details(); }

 protected:
  gfx::Rect CalculateBoundsForDrag(const gfx::Point& location);

  static bool IsBottomEdge(int component);

  // Call during an active resize to change the bounds of the window. This
  // should not be called as the result of a revert.
  void SetBoundsDuringResize(const gfx::Rect& bounds);

  // WindowState of the drag target.
  WindowState* window_state_;

 private:
  // In case of touch resizing, adjusts deltas so that the border is positioned
  // just under the touch point.
  void AdjustDeltaForTouchResize(int* delta_x, int* delta_y);

  // Returns the new origin of the window. The arguments are the difference
  // between the current location and the initial location.
  gfx::Point GetOriginForDrag(int delta_x, int delta_y);

  // Returns the size of the window for the drag.
  gfx::Size GetSizeForDrag(int* delta_x, int* delta_y);

  // Returns the width of the window.
  int GetWidthForDrag(int min_width, int* delta_x);

  // Returns the height of the drag.
  int GetHeightForDrag(int min_height, int* delta_y);

  // Updates |new_bounds| to adhere to the aspect ratio.
  void CalculateBoundsWithAspectRatio(float aspect_ratio,
                                      gfx::Rect* new_bounds);

  std::unique_ptr<PresentationTimeRecorder> recorder_;

  base::WeakPtrFactory<WindowResizer> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WindowResizer);
};

// Creates a WindowResizer for |window|. Returns a unique_ptr with null if
// |window| should not be resized nor dragged.
ASH_EXPORT std::unique_ptr<WindowResizer> CreateWindowResizer(
    aura::Window* window,
    const gfx::Point& point_in_parent,
    int window_component,
    ::wm::WindowMoveSource source);

}  // namespace ash

#endif  // ASH_WM_WINDOW_RESIZER_H_
