// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_DRAG_WINDOW_FROM_SHELF_CONTROLLER_H_
#define ASH_SHELF_DRAG_WINDOW_FROM_SHELF_CONTROLLER_H_

#include <cstdint>
#include <optional>
#include <vector>

#include "ash/ash_export.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/shelf/shelf_metrics.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/splitview/split_view_types.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/timer/timer.h"
#include "ui/aura/window_observer.h"
#include "ui/compositor/layer_tree_owner.h"

namespace aura {
class Window;
}  // namespace aura

namespace gfx {
class PointF;
}  // namespace gfx

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

  // When swiping up from the shelf, the user can continue dragging and end with
  // a downward fling. This is the downward velocity threshold required to
  // restore the original window bounds.
  static constexpr float kVelocityToRestoreBoundsThreshold = 1000.f;

  // The upward velocity threshold to fling the window into overview when split
  // view is active during dragging.
  static constexpr float kVelocityToOverviewThreshold = 1000.f;

  // If the window drag starts within |kDistanceFromEdge| from screen edge, it
  // will get snapped if the drag ends in the snap region, no matter how far
  // the window has been dragged.
  static constexpr int kDistanceFromEdge = 8;

  // A window has to be dragged toward the direction of the edge of the screen
  // for a minimum of |kMinDragDistance| to a point within
  // |kScreenEdgeInsetForSnap| of the edge of the screen, or dragged inside
  // |kDistanceFromEdge| from edge to be snapped.
  static constexpr int kScreenEdgeInsetForSnap = 48;
  static constexpr int kMinDragDistance = 96;

  // The distance for the dragged window to pass over the bottom of the display
  // so that it can be dragged into home launcher or overview. If not pass this
  // value (the top of the hotseat), the window will snap back to its original
  // position. The value is different for standard or dense shelf.
  static float GetReturnToMaximizedThreshold();

  DragWindowFromShelfController(aura::Window* window,
                                const gfx::PointF& location_in_screen);
  DragWindowFromShelfController(const DragWindowFromShelfController&) = delete;
  DragWindowFromShelfController& operator=(
      const DragWindowFromShelfController&) = delete;
  ~DragWindowFromShelfController() override;

  // Called during swiping up on the shelf.
  void Drag(const gfx::PointF& location_in_screen,
            float scroll_x,
            float scroll_y);
  std::optional<ShelfWindowDragResult> EndDrag(
      const gfx::PointF& location_in_screen,
      std::optional<float> velocity_y);
  void CancelDrag();

  bool IsDraggedWindowAnimating() const;

  // Performs the action on the dragged window depending on
  // |window_drag_result_|, such as scaling up/down the dragged window. This
  // method should be called after EndDrag() which computes
  // |window_drag_result_|.
  void FinalizeDraggedWindow();

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;

  aura::Window* dragged_window() { return window_; }
  bool drag_started() const { return drag_started_; }
  bool during_window_restoration() const { return during_window_restoration_; }

 private:
  class WindowsHider;
  friend class DragWindowFromShelfControllerTestApi;

  void OnDragStarted(const gfx::PointF& location_in_screen);
  void OnDragEnded(const gfx::PointF& location_in_screen,
                   bool should_drop_window_in_overview,
                   SnapPosition snap_position);

  // Updates the dragged window's transform during dragging.
  void UpdateDraggedWindow(const gfx::PointF& location_in_screen);

  // Returns the desired snap position on |location_in_screen| during dragging.
  SnapPosition GetSnapPosition(const gfx::PointF& location_in_screen) const;

  // Returns true if the dragged window should restore to its original bounds
  // after drag ends. Happens when the bottom of the dragged window is
  // within the GetReturnToMaximizedThreshold() threshold, or when the downward
  // vertical velocity is larger than kVelocityToRestoreBoundsThreshold.
  bool ShouldRestoreToOriginalBounds(const gfx::PointF& location_in_screen,
                                     std::optional<float> velocity_y) const;

  // Returns true if we should go to home screen after drag ends. Happens when
  // the upward vertical velocity is larger than kVelocityToHomeScreenThreshold
  // and splitview is not active. Note when splitview is active, we do not allow
  // to go to home screen by fling.
  bool ShouldGoToHomeScreen(const gfx::PointF& location_in_screen,
                            std::optional<float> velocity_y) const;

  // Returns the desired snap position on |location_in_screen| when drag ends.
  SnapPosition GetSnapPositionOnDragEnd(const gfx::PointF& location_in_screen,
                                        std::optional<float> velocity_y) const;

  // Returns true if we should drop the dragged window in overview after drag
  // ends.
  bool ShouldDropWindowInOverview(const gfx::PointF& location_in_screen,
                                  std::optional<float> velocity_y) const;

  // Reshows the windows that were hidden before drag starts.
  void ReshowHiddenWindowsOnDragEnd();

  // Calls when the user resumes or ends window dragging. Overview should show
  // up and split view indicators should be updated.
  void ShowOverviewDuringOrAfterDrag();
  // Overview should be hidden when the user drags the window quickly up or
  // around.
  void HideOverviewDuringDrag();

  // Called when the dragged window should scale down and fade out to home
  // screen after drag ends.
  void ScaleDownWindowAfterDrag();

  // Callback function to be called after the window has been scaled down and
  // faded out after drag ends.
  void OnWindowScaledDownAfterDrag();

  // Called when the dragged window should scale up to restore to its original
  // bounds after drag ends.
  void ScaleUpToRestoreWindowAfterDrag();

  // Callback function to be called after the window has been restored to its
  // original bounds after drag ends.
  void OnWindowRestoredToOriginalBounds(bool end_overview);

  // Called to do proper initialization in overview for the dragged window. The
  // function is supposed to be called with an active overview session.
  void OnWindowDragStartedInOverview();

  // Cleans up `other_window_` and `other_window_copy_`.
  // If `show` is `std::nullopt`, we destroy the copy without animation.
  // If `show` is true, drag has been canceled and we scale up the copy and fade
  // it in. The copy will be destroyed and replaced by the original window on
  // animation end.
  // If `show` is false, fade out the copy and destroy it after the animation.
  void ResetOtherWindow(std::optional<bool> show);

  raw_ptr<aura::Window> window_ = nullptr;
  // The `other_window_` refers to the window other than `window_` that is
  // visible while `window_` is being dragged. This happens when there is a
  // floated window.
  raw_ptr<aura::Window> other_window_ = nullptr;
  std::unique_ptr<ui::LayerTreeOwner> other_window_copy_;
  gfx::PointF initial_location_in_screen_;
  gfx::PointF previous_location_in_screen_;
  bool drag_started_ = false;

  // Whether overview was active when the drag started.
  bool started_in_overview_ = false;

  // Hide all eligible windows during window dragging. Depends on different
  // scenarios, we may or may not reshow there windows when drag ends.
  std::unique_ptr<WindowsHider> windows_hider_;

  // Timer to show and update overview.
  base::OneShotTimer show_overview_timer_;

  // True if overview is active and its windows are showing.
  bool show_overview_windows_ = false;

  // A pending action from EndDrag() to be performed in FinalizeDraggedWindow().
  std::optional<ShelfWindowDragResult> window_drag_result_;

  // True while we are restoring windows back to their original bounds after a
  // drag (i.e. dragged tiny amount from shelf).
  bool during_window_restoration_ = false;

  base::OnceClosure on_overview_shown_callback_for_testing_;

  SnapPosition initial_snap_position_ = SnapPosition::kNone;

  SnapPosition end_snap_position_ = SnapPosition::kNone;

  std::unique_ptr<ui::PresentationTimeRecorder> presentation_time_recorder_;

  // Pointer to the last `OverviewSession` that was started by a window drag.
  // Null by default, or if an overview session was started by other means.
  base::WeakPtr<OverviewSession> last_overview_drag_session_ptr_;

  base::WeakPtrFactory<DragWindowFromShelfController> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SHELF_DRAG_WINDOW_FROM_SHELF_CONTROLLER_H_
