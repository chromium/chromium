// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_HOME_SCREEN_DRAG_WINDOW_FROM_SHELF_CONTROLLER_H_
#define ASH_HOME_SCREEN_DRAG_WINDOW_FROM_SHELF_CONTROLLER_H_

#include <vector>

#include "ash/ash_export.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/timer/timer.h"
#include "ui/aura/window_observer.h"
#include "ui/gfx/geometry/point.h"

namespace aura {
class Window;
}

namespace ash {

// The window drag controller that will be used when a window is dragged up by
// swiping up from the shelf to homescreen, overview or splitview.
class ASH_EXPORT DragWindowFromShelfController : public aura::WindowObserver {
 public:
  // The deceleration threshold to open overview behind the dragged window
  // when swiping up from the shelf to drag the active window.
  static constexpr float kOpenOverviewThreshold = 10.f;

  // The deceleration threshold to show or hide overview during window dragging
  // when dragging a window up from the shelf.
  static constexpr float kShowOverviewThreshold = 50.f;

  // The upward velocity threshold to take the user to the home launcher screen
  // when swiping up from the shelf. Can happen anytime during dragging.
  static constexpr float kVelocityToHomeScreenThreshold = 1000.f;

  // The upward velocity threshold to fling the window into overview when split
  // view is active during dragging.
  static constexpr float kVelocityToOverviewThreshold = 1000.f;

  // The distance for the dragged window to pass over the bottom of the display
  // so that it can be dragged into home launcher or overview. If not pass this
  // value, the window will snap back to its original position. The value is
  // different for standard or dense shelf.
  static float GetReturnToMaximizedThreshold();

  enum class ShelfWindowDragResult {
    // Go to home screen after drag ends.
    kGoToHomeScreen,

    // Dragged window restored to its original bounds.
    kRestoreToOriginalBounds,

    // Enter overview mode after drag ends.
    kGoToOverviewMode
  };

  DragWindowFromShelfController(aura::Window* window,
                                const gfx::Point& location_in_screen,
                                HotseatState hotseat_state);
  ~DragWindowFromShelfController() override;

  // Called during swiping up on the shelf.
  void Drag(const gfx::Point& location_in_screen,
            float scroll_x,
            float scroll_y);
  base::Optional<ShelfWindowDragResult> EndDrag(
      const gfx::Point& location_in_screen,
      base::Optional<float> velocity_y);
  void CancelDrag();

  bool IsDraggedWindowAnimating() const;

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;

  aura::Window* dragged_window() const { return window_; }
  bool drag_started() const { return drag_started_; }

 private:
  class WindowsHider;

  void OnDragStarted(const gfx::Point& location_in_screen);
  void OnDragEnded(const gfx::Point& location_in_screen,
                   bool should_drop_window_in_overview,
                   SplitViewController::SnapPosition snap_position);

  // Updates the dragged window's transform during dragging.
  void UpdateDraggedWindow(const gfx::Point& location_in_screen);

  // Returns the desired snap position on |location_in_screen| during dragging.
  SplitViewController::SnapPosition GetSnapPosition(
      const gfx::Point& location_in_screen) const;

  // Returns true if the dragged window should restore to its original bounds
  // after drag ends. Happens when |location_in_screen| is within
  // GetReturnToMaximizedThreshold() threshold.
  bool ShouldRestoreToOriginalBounds(
      const gfx::Point& location_in_screen) const;

  // Returns true if we should go to home screen after drag ends. Happens when
  // the upward vertical velocity is larger than kVelocityToHomeScreenThreshold
  // and splitview is not active. Note when splitview is active, we do not allow
  // to go to home screen by fling.
  bool ShouldGoToHomeScreen(const gfx::Point& location_in_screen,
                            base::Optional<float> velocity_y) const;

  // Returns the desired snap position on |location_in_screen| when drag ends.
  SplitViewController::SnapPosition GetSnapPositionOnDragEnd(
      const gfx::Point& location_in_screen,
      base::Optional<float> velocity_y) const;

  // Returns true if we should drop the dragged window in overview after drag
  // ends.
  bool ShouldDropWindowInOverview(const gfx::Point& location_in_screen,
                                  base::Optional<float> velocity_y) const;

  // Reshows the windows that were hidden before drag starts.
  void ReshowHiddenWindowsOnDragEnd();

  // Calls when the user resumes or ends window dragging. Overview should show
  // up and split view indicators should be updated.
  void ShowOverviewDuringOrAfterDrag();

  // Called when the dragged window should scale down and fade out to home
  // screen after drag ends.
  void ScaleDownWindowAfterDrag();

  // Called when the dragged window should scale up to restore to its original
  // bounds after drag ends.
  void ScaleUpToRestoreWindowAfterDrag();

  // Callback function to be called after the window has been restored to its
  // original bounds after drag ends.
  void OnWindowRestoredToOrignalBounds(bool end_overview);

  // Called to do proper initialization in overview for the dragged window. The
  // function is supposed to be called with an active overview session.
  void OnWindowDragStartedInOverview();

  aura::Window* window_ = nullptr;
  gfx::Point initial_location_in_screen_;
  gfx::Point previous_location_in_screen_;
  bool drag_started_ = false;
  BackdropWindowMode original_backdrop_mode_ = BackdropWindowMode::kAutoOpaque;

  // Hide all eligible windows during window dragging. Depends on different
  // scenarios, we may or may not reshow there windows when drag ends.
  std::unique_ptr<WindowsHider> windows_hider_;

  // Timer to show and update overview.
  base::OneShotTimer show_overview_timer_;

  // The hotseat state when drag starts.
  const HotseatState hotseat_state_;

  base::WeakPtrFactory<DragWindowFromShelfController> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DragWindowFromShelfController);
};

}  // namespace ash

#endif  // ASH_HOME_SCREEN_DRAG_WINDOW_FROM_SHELF_CONTROLLER_H_
