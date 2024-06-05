// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_OVERVIEW_WINDOW_DRAG_CONTROLLER_H_
#define ASH_WM_OVERVIEW_OVERVIEW_WINDOW_DRAG_CONTROLLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/splitview/split_view_types.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size_f.h"

namespace aura {
class Window;
}  // namespace aura

namespace ui {
class PresentationTimeRecorder;
}  // namespace ui

namespace ash {

class OverviewGrid;
class OverviewItemBase;
class OverviewSession;
class SplitViewController;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Workflows of dragging windows from overview (not from the top or shelf).
enum class OverviewDragAction {
  kToGridSameDisplayClamshellMouse = 0,
  kToGridSameDisplayClamshellTouch = 1,
  kToDeskSameDisplayClamshellMouse = 2,
  kToDeskSameDisplayClamshellTouch = 3,
  kToSnapSameDisplayClamshellMouse = 4,
  kToSnapSameDisplayClamshellTouch = 5,
  kSwipeToCloseSuccessfulClamshellTouch = 6,
  kSwipeToCloseCanceledClamshellTouch = 7,
  kFlingToCloseClamshellTouch = 8,
  kToGridOtherDisplayClamshellMouse = 9,
  kToDeskOtherDisplayClamshellMouse = 10,
  kToSnapOtherDisplayClamshellMouse = 11,
  kToGridSameDisplayTabletTouch = 12,
  kToDeskSameDisplayTabletTouch = 13,
  kToSnapSameDisplayTabletTouch = 14,
  kSwipeToCloseSuccessfulTabletTouch = 15,
  kSwipeToCloseCanceledTabletTouch = 16,
  kFlingToCloseTabletTouch = 17,
  kMaxValue = kFlingToCloseTabletTouch,
};

// The drag controller for an overview window item in overview mode. It updates
// the position of the corresponding window item using transform while dragging.
// It also updates the split view drag indicators, which handles showing
// indicators where to drag, and preview areas showing the bounds of the
// window about to be snapped.
class ASH_EXPORT OverviewWindowDragController {
 public:
  enum class DragBehavior {
    // No drag has started.
    kNoDrag,
    // Drag has started, but it is undecided whether we want to drag to snap or
    // drag to close yet.
    kUndefined,
    // On drag complete, the window will be snapped, if it meets requirements,
    // or moved to another desk if dropped on one of the desks' mini_views. This
    // mode is triggered if the the window is initially dragged horizontally
    // more than vertically (more in X than Y), or if the window item in the
    // overview grid was gesture long pressed.
    kNormalDrag,
    // On drag complete, the window will be closed, if it meets requirements.
    // This mode is triggered when the window is initially dragged vertically
    // more than horizontally (more in Y than in X).
    kDragToClose,
  };

  enum class DragResult {
    // The drag ended without ever being disambiguated between a normal drag and
    // drag-to-close.
    kNeverDisambiguated,
    // The drag was considered as a normal drag, and then the window was dropped
    // back into overview, in the same grid or another one.
    kDropIntoOverview,
    // The drag resulted in snapping the window.
    kSnap,
    // The drag resulted in moving the window to another desk.
    kDragToDesk,
    // The drag resulted in closing the window.
    kSuccessfulDragToClose,
    // The drag was considered as drag-to-close, but did not result in closing
    // the window.
    kCanceledDragToClose,
  };

  OverviewWindowDragController(OverviewSession* overview_session,
                               OverviewItemBase* item,
                               bool is_touch_dragging,
                               OverviewItemBase* event_source_item);

  OverviewWindowDragController(const OverviewWindowDragController&) = delete;
  OverviewWindowDragController& operator=(const OverviewWindowDragController&) =
      delete;

  ~OverviewWindowDragController();

  static base::AutoReset<bool> SkipNewDeskButtonScaleUpDurationForTesting();

  OverviewItemBase* item() { return item_; }

  bool is_touch_dragging() const { return is_touch_dragging_; }

  void InitiateDrag(const gfx::PointF& location_in_screen);
  void Drag(const gfx::PointF& location_in_screen);
  DragResult CompleteDrag(const gfx::PointF& location_in_screen);
  void StartNormalDragMode(const gfx::PointF& location_in_screen);
  DragResult Fling(const gfx::PointF& location_in_screen,
                   float velocity_x,
                   float velocity_y);
  void ActivateDraggedWindow();

  // Called when a gesture event is reset or when the dragged window is being
  // destroyed.
  void ResetGesture();

  // Resets |overview_session_| to nullptr. It's needed since we defer the
  // deletion of OverviewWindowDragController in Overview destructor and
  // we need to reset |overview_session_| to nullptr to avoid null pointer
  // dereference.
  void ResetOverviewSession();

  DragBehavior current_drag_behavior_for_testing() const {
    return current_drag_behavior_;
  }

  base::OneShotTimer* new_desk_button_scale_up_timer_for_test() {
    return &new_desk_button_scale_up_timer_;
  }

 private:
  enum NormalDragAction {
    kToGrid = 0,
    kToDesk = 1,
    kToSnap = 2,
    kNormalDragActionEnumSize = 3,
  };
  enum DragToCloseAction {
    kSwipeToCloseSuccessful = 0,
    kSwipeToCloseCanceled = 1,
    kFlingToClose = 2,
    kDragToCloseActionEnumSize = 3,
  };

  void StartDragToCloseMode();

  // Methods to continue and complete the drag when the drag mode is
  // kDragToClose.
  void ContinueDragToClose(const gfx::PointF& location_in_screen);
  DragResult CompleteDragToClose(const gfx::PointF& location_in_screen);

  // Methods to continue and complete the drag when the drag mode is
  // kNormalDrag.
  void ContinueNormalDrag(const gfx::PointF& location_in_screen);
  DragResult CompleteNormalDrag(const gfx::PointF& location_in_screen);

  // Updates visuals for the user while dragging items around.
  void UpdateDragIndicatorsAndOverviewGrid(
      const gfx::PointF& location_in_screen);

  aura::Window* GetRootWindowBeingDraggedIn() const;

  SnapPosition GetSnapPosition(const gfx::PointF& location_in_screen) const;

  // Snaps and activates the window. Uses the divider spawn animation (see
  // |SplitViewController::SnapWindow|). Sets |item_| to null because the
  // overview item is destroyed.
  void SnapWindow(SplitViewController* split_view_controller,
                  SnapPosition snap_position);

  // Returns the item's overview grid, or the grid in which the item is being
  // dragged if the multi display overview and split view feature is enabled.
  OverviewGrid* GetCurrentGrid() const;

  // Records the histogram Ash.Overview.WindowDrag.Workflow.
  void RecordNormalDrag(NormalDragAction action,
                        bool is_dragged_to_other_display) const;
  void RecordDragToClose(DragToCloseAction action) const;

  // Creates `float_drag_helper_` if needed. The helper will temporarily stack
  // the float container under the active desk container, so that dragging
  // regular windows appear above overview items of floated windows.
  void MaybeCreateFloatDragHelper();

  // Scale up the new desk button on the desks bar from expanded state to active
  // state to make the new desk button a drop target for the window being
  // dragged. It's triggered by `new_desk_button_scale_up_timer_`. Refer to
  // `new_desk_button_scale_up_timer_` for more information.
  void MaybeScaleUpNewDeskButton();

  raw_ptr<OverviewSession> overview_session_;

  // The drag target item in the overview mode.
  raw_ptr<OverviewItemBase, DanglingUntriaged> item_ = nullptr;

  // The source item of the drag event.
  raw_ptr<OverviewItemBase, DanglingUntriaged> event_source_item_ = nullptr;

  DragBehavior current_drag_behavior_ = DragBehavior::kNoDrag;

  // The location of the initial mouse/touch/gesture event in screen.
  gfx::PointF initial_event_location_;

  // Stores the bounds of |item_| when a drag is started. Used to calculate the
  // new bounds on a drag event.
  gfx::PointF initial_centerpoint_;

  // The original size of the dragged item after we scale it up when we start
  // dragging it. The item is restored to this size once it no longer intersects
  // with the OverviewDeskBarView.
  gfx::SizeF original_scaled_size_;

  // Track the per-overview-grid desks bar data used to perform the window
  // sizing operations when it is moved towards or on the desks bar.
  struct GridDesksBarData {
    // The scaled-down size of the dragged item once the drag location is on the
    // OverviewDeskBarView of the corresponding grid. We size the item down so
    // that it fits inside the desks' preview view.
    gfx::SizeF on_desks_bar_item_size;

    // Cached values related to dragging items while the desks bar is shown.
    // |desks_bar_bounds| is the bounds of the desks bar in screen coordinates.
    // |shrink_bounds| is a rectangle around the desks bar which the items
    // starts shrinking when the event location is contained. The item will
    // shrink until it is contained in |desks_bar_bounds|, at which it has
    // reached its minimum size and will no longer shrink.
    // |shrink_region_distance| is a vector contained the distance from the
    // origin of |desks_bar_bounds| to the origin of |shrink_bounds|. It's
    // used to determine the size of the dragged item when it's within
    // |shrink_bounds|.
    gfx::RectF desks_bar_bounds;
    gfx::RectF shrink_bounds;
    gfx::Vector2dF shrink_region_distance;
  };
  base::flat_map<OverviewGrid*, GridDesksBarData> per_grid_desks_bar_data_;

  const size_t display_count_;

  // Indicates touch dragging, as opposed to mouse dragging. The drag-to-close
  // mode is only allowed when |is_touch_dragging_| is true.
  const bool is_touch_dragging_;

  // True if the `item_` can be snapped by dragging.
  const bool is_eligible_for_drag_to_snap_;

  // True if the Virtual Desks bar is created and dragging to desks is enabled.
  const bool virtual_desks_bar_enabled_;

  // The opacity of |item_| changes if we are in drag to close mode. Store the
  // original opacity of |item_| and restore it to the item when we leave drag
  // to close mode.
  float original_opacity_ = 1.f;

  // Set to true once the bounds of |item_| change.
  bool did_move_ = false;

  // Records the presentation time of window drag operation in overview mode.
  std::unique_ptr<ui::PresentationTimeRecorder> presentation_time_recorder_;

  SnapPosition snap_position_ = SnapPosition::kNone;

  std::optional<OverviewController::ScopedOcclusionPauser> occlusion_pauser_;

  // A timer used to scale up the new desk button to make it a drop target for
  // the window being dragged if the window is hovered on the button over a
  // period of time.
  base::OneShotTimer new_desk_button_scale_up_timer_;
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_OVERVIEW_WINDOW_DRAG_CONTROLLER_H_
