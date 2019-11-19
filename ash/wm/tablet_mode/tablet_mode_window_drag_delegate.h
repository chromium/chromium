// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_TABLET_MODE_TABLET_MODE_WINDOW_DRAG_DELEGATE_H_
#define ASH_WM_TABLET_MODE_TABLET_MODE_WINDOW_DRAG_DELEGATE_H_

#include <memory>

#include "ash/public/cpp/presentation_time_recorder.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/toplevel_window_event_handler.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "ui/aura/window_occlusion_tracker.h"
#include "ui/wm/core/shadow_types.h"

namespace ash {

class SplitViewDragIndicators;
class PresentationTimeRecorder;

// This class includes the common logic when dragging a window around, either
// it's a browser window, or an app window. It does almost everything needs to
// be done, including updating the dragged window's bounds (if it's dragged by
// tabs) or transform (if it's dragged by entire window). it also decides
// when/where to show the drag indicators and preview windows, shows/hides the
// backdrop, interacts with overview and splitview, etc.
class TabletModeWindowDragDelegate {
 public:
  // The threshold to compute the end position of the dragged window to drop it
  // into overview.
  static constexpr float kDragPositionToOverviewRatio = 0.5f;

  // Threshold of the fling velocity to drop the dragged window into overview if
  // fling from the top of the display or from the caption area of the window.
  static constexpr float kFlingToOverviewThreshold = 2000.f;

  // Threshold of the fling velocity to drop the dragged window into overview if
  // fling inside preview area or when splitview is active.
  static constexpr float kFlingToOverviewFromSnappingAreaThreshold = 1000.f;

  enum class UpdateDraggedWindowType {
    UPDATE_BOUNDS,
    UPDATE_TRANSFORM,
  };

  TabletModeWindowDragDelegate();
  virtual ~TabletModeWindowDragDelegate();

  // Called when a window starts being dragged.
  void StartWindowDrag(aura::Window* window,
                       const gfx::Point& location_in_screen);

  // Called when a window continues being dragged. |type| specifies how we want
  // to update the dragged window during dragging, and |target_bounds| is the
  // target window bounds for the dragged window if |type| is UPDATE_BOUNDS.
  // Note |target_bounds| has no use if |type| is UPDATE_TRANSFROM.
  void ContinueWindowDrag(const gfx::Point& location_in_screen,
                          UpdateDraggedWindowType type,
                          const gfx::Rect& target_bounds = gfx::Rect());

  // Calls when a window ends dragging with its drag result |result|.
  void EndWindowDrag(ToplevelWindowEventHandler::DragResult result,
                     const gfx::Point& location_in_screen);

  // Calls when a window ends dragging because of fling or swipe.
  void FlingOrSwipe(ui::GestureEvent* event);

  // Return the location of |event| in screen coordinates.
  gfx::Point GetEventLocationInScreen(const ui::GestureEvent* event) const;

  aura::Window* dragged_window() { return dragged_window_; }

  SplitViewDragIndicators* split_view_drag_indicators_for_testing() {
    return split_view_drag_indicators_.get();
  }

  void set_drag_start_deadline_for_testing(base::Time time) {
    drag_start_deadline_ = time;
  }

 protected:
  // These five methods are used by its child class to do its special handling
  // before/during/after dragging.
  virtual void PrepareWindowDrag(const gfx::Point& location_in_screen) {}
  virtual void UpdateWindowDrag(const gfx::Point& location_in_screen) {}
  virtual void EndingWindowDrag(ToplevelWindowEventHandler::DragResult result,
                                const gfx::Point& location_in_screen) {}
  virtual void EndedWindowDrag(const gfx::Point& location_in_screen) {}
  // Calls when a fling event starts.
  virtual void StartFling(const ui::GestureEvent* event) {}

  // Returns true if we should open overview behind the dragged window when drag
  // starts.
  virtual bool ShouldOpenOverviewWhenDragStarts();

  // When the dragged window is dragged past this value, the drag indicators
  // will show up.
  int GetIndicatorsVerticalThreshold(const gfx::Rect& work_area_bounds) const;

  // Gets the desired snap position for |location_in_screen|.
  SplitViewController::SnapPosition GetSnapPosition(
      const gfx::Point& location_in_screen) const;

  // Updates the dragged window's transform during dragging.
  void UpdateDraggedWindowTransform(const gfx::Point& location_in_screen);

  // Returns true if the dragged window should be dropped into overview on drag
  // end.
  bool ShouldDropWindowIntoOverview(
      SplitViewController::SnapPosition snap_position,
      const gfx::Point& location_in_screen);

  // Returns true if fling event should drop the window into overview grid.
  bool ShouldFlingIntoOverview(const ui::GestureEvent* event) const;

  // Updates |is_window_considered_moved_| on current time and
  // |y_location_in_screen|.
  void UpdateIsWindowConsideredMoved(int y_location_in_screen);

  SplitViewController* const split_view_controller_;

  // A widget to display the drag indicators and preview window.
  std::unique_ptr<SplitViewDragIndicators> split_view_drag_indicators_;

  aura::Window* dragged_window_ = nullptr;  // not owned.

  // The backdrop should be disabled during dragging and resumed after dragging.
  BackdropWindowMode original_backdrop_mode_ = BackdropWindowMode::kAutoOpaque;

  // The dragged window should have the active window shadow elevation during
  // dragging.
  int original_shadow_elevation_ = ::wm::kShadowElevationDefault;

  gfx::Point initial_location_in_screen_;

  // Overview mode will be triggered if a window is being dragged, and the drop
  // target will be created in the overview grid. The variable stores the bounds
  // of the selected drop target in overview and will be used to calculate the
  // desired window transform during dragging.
  gfx::Rect bounds_of_selected_drop_target_;

  // True if the |dragged_window_| has been considered as moved. Only after it
  // has been dragged longer than kIsWindowMovedTimeoutMs on time and further
  // than GetIndicatorsVerticalThreshold on distance, it can be considered as
  // moved. Only change its window state or show the drag indicators if it has
  // been 'moved'. Once it has been 'moved', it will stay as 'moved'.
  bool is_window_considered_moved_ = false;

  // Drag need to last later than the deadline here to be considered as 'moved'.
  base::Time drag_start_deadline_;

  base::Optional<aura::WindowOcclusionTracker::ScopedExclude>
      occlusion_excluder_;

  // Records the presentation time for app/browser/tab window dragging
  // in tablet mode.
  std::unique_ptr<PresentationTimeRecorder> presentation_time_recorder_;

  base::WeakPtrFactory<TabletModeWindowDragDelegate> weak_ptr_factory_{this};

 private:
  DISALLOW_COPY_AND_ASSIGN(TabletModeWindowDragDelegate);
};

}  // namespace ash

#endif  // ASH_WM_TABLET_MODE_TABLET_MODE_WINDOW_DRAG_DELEGATE_H_
