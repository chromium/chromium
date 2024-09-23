// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WORKSPACE_WORKSPACE_WINDOW_RESIZER_H_
#define ASH_WM_WORKSPACE_WORKSPACE_WINDOW_RESIZER_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "ash/wm/window_resizer.h"
#include "ash/wm/workspace/magnetism_matcher.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "ui/aura/window_tracker.h"
#include "ui/compositor/presentation_time_recorder.h"
#include "ui/display/display.h"
#include "ui/gfx/geometry/point_f.h"

namespace ash {
class WindowSplitter;
class PhantomWindowController;
class WindowSize;
class WindowState;

// WindowResizer implementation for workspaces. This enforces that windows are
// not allowed to vertically move or resize outside of the work area. As windows
// are moved outside the work area they are shrunk. We remember the height of
// the window before it was moved so that if the window is again moved up we
// attempt to restore the old height.
class ASH_EXPORT WorkspaceWindowResizer : public WindowResizer {
 public:
  // Possible states the window can end up in after a drag is complete.
  enum class SnapType { kPrimary, kSecondary, kMaximize, kNone };

  // Min height we'll force on screen when dragging the caption.
  // TODO: this should come from a property on the window.
  static constexpr int kMinOnscreenHeight = 32;

  ~WorkspaceWindowResizer() override;

  static std::unique_ptr<WorkspaceWindowResizer> Create(
      WindowState* window_state,
      const std::vector<raw_ptr<aura::Window, VectorExperimental>>&
          attached_windows);

  // WindowResizer:
  void Drag(const gfx::PointF& location_in_parent, int event_flags) override;
  void CompleteDrag() override;
  void RevertDrag() override;
  void FlingOrSwipe(ui::GestureEvent* event) override;

 private:
  friend class WorkspaceWindowResizerTest;
  friend class WorkspaceWindowResizerTestApi;
  FRIEND_TEST_ALL_PREFIXES(HapticsUtilTest, HapticFeedbackForNormalWindowSnap);

  WorkspaceWindowResizer(
      WindowState* window_state,
      const std::vector<raw_ptr<aura::Window, VectorExperimental>>&
          attached_windows);
  WorkspaceWindowResizer(const WorkspaceWindowResizer&) = delete;
  WorkspaceWindowResizer& operator=(const WorkspaceWindowResizer&) = delete;

  // Lays out the attached windows. |bounds| is the bounds of the main window.
  void LayoutAttachedWindows(gfx::Rect* bounds);

  // Calculates the new sizes of the attached windows, given that the main
  // window has been resized (along the primary axis) by |delta|.
  // |available_size| is the maximum length of the space that the attached
  // windows are allowed to occupy (ie: the distance between the right/bottom
  // edge of the primary window and the right/bottom of the desktop area).
  // Populates |sizes| with the desired sizes of the attached windows, and
  // returns the number of pixels that couldn't be allocated to the attached
  // windows (due to min/max size constraints).
  // Note the return value can be positive or negative, a negative value
  // indicating that that many pixels couldn't be removed from the attached
  // windows.
  int CalculateAttachedSizes(int delta,
                             int available_size,
                             std::vector<int>* sizes) const;

  // Divides |amount| evenly between |sizes|. If |amount| is negative it
  // indicates how many pixels |sizes| should be shrunk by.
  // Returns how many pixels failed to be allocated/removed from |sizes|.
  int GrowFairly(int amount, std::vector<WindowSize>* sizes) const;

  // Calculate the ratio of pixels that each WindowSize in |sizes| should
  // receive when growing or shrinking.
  void CalculateGrowthRatios(const std::vector<WindowSize*>& sizes,
                             std::vector<float>* out_ratios) const;

  // Adds a WindowSize to |sizes| for each attached window.
  void CreateBucketsForAttached(std::vector<WindowSize>* sizes) const;

  // If possible snaps the window to a neary window in |display|. Updates
  // |bounds| if there was a close enough window. |display| should be the
  // display containing the last event location.
  void MagneticallySnapToOtherWindows(const display::Display& display,
                                      gfx::Rect* bounds);

  // If possible snaps the resize to a neary window in |display|. Updates
  // |bounds| if there was a close enough window. |display| should be the
  // display containing the last event location.
  void MagneticallySnapResizeToOtherWindows(const display::Display& display,
                                            gfx::Rect* bounds);

  // Finds the neareset window in |display| to magentically snap to. Updates
  // |magnetism_window_| and |magnetism_edge_| appropriately. |edges| is a
  // bitmask of the MagnetismEdges to match again. Returns true if a match is
  // found.
  bool UpdateMagnetismWindow(const display::Display& display,
                             const gfx::Rect& bounds,
                             uint32_t edges);

  // Adjusts the bounds of the window: magnetically snapping, ensuring the
  // window has enough on screen... |snap_size| is the distance from an edge of
  // the work area before the window is snapped. A value of 0 results in no
  // snapping.
  void AdjustBoundsForMainWindow(int snap_size, gfx::Rect* bounds);

  // Stick the window bounds to the work area during a move.
  bool StickToWorkAreaOnMove(const gfx::Rect& work_area,
                             int sticky_size,
                             gfx::Rect* bounds) const;

  // Stick the window bounds to the work area during a resize.
  void StickToWorkAreaOnResize(const gfx::Rect& work_area,
                               int sticky_size,
                               gfx::Rect* bounds) const;

  // Returns a coordinate along the primary axis. Used to share code for
  // left/right multi window resize and top/bottom resize.
  int PrimaryAxisSize(const gfx::Size& size) const;
  int PrimaryAxisCoordinate(int x, int y) const;

  // From a given snap |type| and the current |display|, returns true if the
  // snap is snap top or maximize. This function is used to decide if we need
  // to show the phantom window for top snap or maximize or not.
  bool IsSnapTopOrMaximize(SnapType type,
                           const display::Display& display) const;

  // Updates the bounds of the phantom window where the snap bounds are
  // calculated from GetSnappedWindowBounds() given a |target_snap_type| and
  // maximize bounds is from the current |display|'s full work area.
  void UpdateSnapPhantomWindow(const SnapType target_snap_type,
                               const display::Display& display);

  // Restacks the windows z-order position so that one of the windows is at the
  // top of the z-order, and the rest directly underneath it.
  void RestackWindows();

  // Returns the edge to which the window should be snapped to if the user does
  // no more dragging. kSnapNone is returned if the window should not be
  // snapped, whether it has not been dragged to the correct region, or the
  // window does not allow for snapping.
  SnapType GetSnapType(const display::Display& display,
                       const gfx::PointF& location_in_screen) const;

  // Returns true if |window| bounds are valid bounds for a snap state and snap
  // ratio in |window_state_|.
  bool AreBoundsValidSnappedBounds(aura::Window* window) const;

  // Sets |window|'s state type to |new_state_type|. Called after the drag has
  // been completed for fling/swipe gestures.
  void SetWindowStateTypeFromGesture(aura::Window* window,
                                     chromeos::WindowStateType new_state_type);

  // Start/End drag for attached windows if there is any.
  void StartDragForAttachedWindows();
  void EndDragForAttachedWindows(bool revert_drag);

  WindowState* window_state() { return window_state_; }
  const WindowState* window_state() const { return window_state_; }

  // Returns the currently used instance for test.
  static WorkspaceWindowResizer* GetInstanceForTest();

  const std::vector<raw_ptr<aura::Window, VectorExperimental>>
      attached_windows_;

  bool did_lock_cursor_ = false;

  // Set to true once Drag() is invoked and the bounds of the window change.
  bool did_move_or_resize_ = false;

  // Tracks whether a window can be maximized depending on distance dragged.
  // Set to true when Drag() has a vertical move more than
  // kSnapTriggerVerticalMoveThreshold.
  bool can_snap_to_maximize_ = false;

  // True if the window initially had |bounds_changed_by_user_| set in state.
  const bool initial_bounds_changed_by_user_;

  // The initial size of each of the windows in |attached_windows_| along the
  // primary axis.
  std::vector<int> initial_size_;

  // Sum of the minimum sizes of the attached windows.
  int total_min_ = 0;

  // Sum of the sizes in |initial_size_|.
  int total_initial_size_ = 0;

  // Gives a previews of where the the window will end up. Only used if there
  // is a grid and the caption is being dragged.
  std::unique_ptr<PhantomWindowController> snap_phantom_window_controller_;

  // The edge to which the window should be snapped to at the end of the drag.
  SnapType snap_type_ = SnapType::kNone;

  // Timer for dwell time countdown.
  base::OneShotTimer dwell_countdown_timer_;
  // The location for drag maximize in screen.
  std::optional<gfx::PointF> dwell_location_in_screen_;

  // The latest location passed to `Drag()` in screen coordinates.
  gfx::PointF last_location_in_screen_;

  // Window the drag has magnetically attached to.
  raw_ptr<aura::Window> magnetism_window_ = nullptr;

  // Used to verify |magnetism_window_| is still valid.
  aura::WindowTracker window_tracker_;

  // If |magnetism_window_| is non-NULL this indicates how the two windows
  // should attach.
  MatchedEdge magnetism_edge_;

  // The window bounds when the drag was started. When a window is minimized,
  // maximized or snapped via a swipe/fling gesture, the restore bounds should
  // be set to the bounds of the window when the drag was started. If the window
  // started with restore bounds (snapped/maximized), those will be used
  // instead.
  gfx::Rect restore_bounds_for_gesture_;

  // Presentation time recorder for tab dragging in clamshell mode.
  std::unique_ptr<ui::PresentationTimeRecorder> tab_dragging_recorder_;

  // Optional window splitter for tiling groups.
  std::unique_ptr<WindowSplitter> window_splitter_;

  // Used to determine if this has been deleted during a drag such as when a tab
  // gets dragged into another browser window.
  base::WeakPtrFactory<WorkspaceWindowResizer> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_WM_WORKSPACE_WORKSPACE_WINDOW_RESIZER_H_
