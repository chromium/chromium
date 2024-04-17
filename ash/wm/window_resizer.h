// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_RESIZER_H_
#define ASH_WM_WINDOW_RESIZER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/wm/drag_details.h"
#include "ash/wm/window_state.h"
#include "base/memory/raw_ptr.h"
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
class PresentationTimeRecorder;

// WindowResizer is used by ToplevelWindowEventHandler to handle dragging,
// moving or resizing a window. All coordinates passed to this are in the parent
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

  WindowResizer(const WindowResizer&) = delete;
  WindowResizer& operator=(const WindowResizer&) = delete;

  virtual ~WindowResizer();

  // Returns a bitmask of the `kBoundsChange_*` values.
  static int GetBoundsChangeForWindowComponent(int component);

  // Returns a bitmask of the `kBoundsChangeDirection_*` values.
  static int GetPositionChangeDirectionForWindowComponent(int window_component);

  // Invoked to drag/move/resize the window. |location| is in the coordinates
  // of the window supplied to the constructor. |event_flags| is the event
  // flags from the event.
  virtual void Drag(const gfx::PointF& location, int event_flags) = 0;

  // Invoked during pinch to move and resize the window. `location` is in the
  // coordinates of the window supplied to the constructor. `scale` is the
  // the scale change since last gesture event.
  virtual void Pinch(const gfx::PointF& location, float scale) {}

  // Invoked to complete the drag.
  virtual void CompleteDrag() = 0;

  // Reverts the drag.
  virtual void RevertDrag() = 0;

  // Flings or Swipes to end the drag.
  virtual void FlingOrSwipe(ui::GestureEvent* event) = 0;

  // Returns the target window the resizer was created for.
  aura::Window* GetTarget() const;

  // See comment for |DragDetails::initial_location_in_parent|.
  const gfx::PointF& GetInitialLocation() const {
    return window_state_->drag_details()->initial_location_in_parent;
  }

  // Drag parameters established when drag starts.
  const DragDetails& details() const { return *window_state_->drag_details(); }

 protected:
  gfx::Rect CalculateBoundsForDrag(const gfx::PointF& location);

  // Call during an active resize to change the bounds of the window. This
  // should not be called as the result of a revert.
  void SetBoundsDuringResize(const gfx::Rect& bounds);

  // Called during an active resize to change the transform of the
  // window.
  void SetTransformDuringResize(const gfx::Transform& transform);

  void SetPresentationTimeRecorder(
      std::unique_ptr<PresentationTimeRecorder> recorder);

  // WindowState of the drag target.
  raw_ptr<WindowState> window_state_;

 private:
  // In case of touch resizing, adjusts deltas so that the border is positioned
  // just under the touch point.
  void AdjustDeltaForTouchResize(int* delta_x, int* delta_y);

  // Returns the new origin of the window. |delta_x| and |delta_y| are the
  // difference between the current location and the initial location.
  // |event_location| is the current location of the mouse or touch event.
  gfx::Point GetOriginForDrag(int delta_x,
                              int delta_y,
                              const gfx::PointF& event_location);

  // Returns the size of the window for the drag.
  gfx::Size GetSizeForDrag(int* delta_x, int* delta_y) const;

  // Called by `GetSizeForDrag` to get the width of the window for the drag.
  int GetWidthForDrag(int min_width, int* delta_x) const;

  // Called by `GetSizeForDrag` to get the height of the window for the drag.
  int GetHeightForDrag(int min_height, int* delta_y) const;

  // Updates |new_bounds| to adhere to the aspect ratio.
  void CalculateBoundsWithAspectRatio(float aspect_ratio,
                                      gfx::Rect* new_bounds);

  std::unique_ptr<PresentationTimeRecorder> recorder_;

  base::WeakPtrFactory<WindowResizer> weak_ptr_factory_{this};
};

// Creates a WindowResizer for |window|. Returns a unique_ptr with null if
// |window| should not be resized nor dragged.
ASH_EXPORT std::unique_ptr<WindowResizer> CreateWindowResizer(
    aura::Window* window,
    const gfx::PointF& point_in_parent,
    int window_component,
    ::wm::WindowMoveSource source);

}  // namespace ash

#endif  // ASH_WM_WINDOW_RESIZER_H_
