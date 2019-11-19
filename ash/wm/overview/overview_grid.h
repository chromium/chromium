// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_OVERVIEW_GRID_H_
#define ASH_WM_OVERVIEW_OVERVIEW_GRID_H_

#include <stddef.h>

#include <memory>
#include <vector>

#include "ash/public/cpp/wallpaper_controller_observer.h"
#include "ash/rotator/screen_rotation_animator_observer.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/splitview/split_view_drag_indicators.h"
#include "ash/wm/splitview/split_view_observer.h"
#include "ash/wm/window_state.h"
#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "ui/aura/window.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"

namespace views {
class Widget;
}

namespace ash {

class DesksBarView;
class FpsCounter;
class OverviewGridEventHandler;
class OverviewItem;
class PresentationTimeRecorder;

// Represents a grid of windows in the Overview Mode in a particular root
// window, and manages a selection widget that can be moved with the arrow keys.
// The idea behind the movement strategy is that it should be possible to access
// any window pressing a given arrow key repeatedly.
// +-------+  +-------+  +-------+
// |   0   |  |   1   |  |   2   |
// +-------+  +-------+  +-------+
// +-------+  +-------+  +-------+
// |   3   |  |   4   |  |   5   |
// +-------+  +-------+  +-------+
// +-------+
// |   6   |
// +-------+
// Example sequences:
//  - Going right to left
//    0, 1, 2, 3, 4, 5, 6
// The selector is switched to the next window grid (if available) or wrapped if
// it reaches the end of its movement sequence.
class ASH_EXPORT OverviewGrid : public SplitViewObserver,
                                public ScreenRotationAnimatorObserver,
                                public WallpaperControllerObserver {
 public:
  OverviewGrid(aura::Window* root_window,
               const std::vector<aura::Window*>& window_list,
               OverviewSession* overview_session);
  ~OverviewGrid() override;

  // Exits overview mode.
  void Shutdown();

  // Prepares the windows in this grid for overview. This will restore all
  // minimized windows and ensure they are visible.
  void PrepareForOverview();

  // Positions all the windows in rows of equal height scaling each window to
  // fit that height. Optionally animates the windows to their targets when
  // |animate| is true. Items in |ignored_items| are not positioned. This is for
  // dragging. |transition| specifies the overview state when this function is
  // called.
  void PositionWindows(bool animate,
                       const base::flat_set<OverviewItem*>& ignored_items = {},
                       OverviewSession::OverviewTransition transition =
                           OverviewSession::OverviewTransition::kInOverview);

  // Returns the OverviewItem if a window is contained in any of the
  // OverviewItems this grid owns. Returns nullptr if no such a OverviewItem
  // exist.
  OverviewItem* GetOverviewItemContaining(const aura::Window* window) const;

  // Adds |window| at the specified |index|. |window| cannot already be on the
  // grid. If |reposition| is true, repositions all items except those in
  // |ignored_items|. If |animate| is true, animates the repositioning.
  // |animate| has no effect if |reposition| is false.
  // If |use_spawn_animation| is true, and this item is being added *while*
  // overview is already active, it will use a special spawn animation on its
  // first position in the grid. |use_spawn_animation| has no effect if either
  // |animate| or |reposition| are false.
  //
  // Note: This function should only be called by |OverviewSession::AddItem|.
  // |overview_session_| keeps count of all overview items, but this function
  // does not update the tally.
  void AddItem(aura::Window* window,
               bool reposition,
               bool animate,
               const base::flat_set<OverviewItem*>& ignored_items,
               size_t index,
               bool use_spawn_animation = false);

  // Similar to the above function, but adds the window to the end of the grid.
  void AppendItem(aura::Window* window,
                  bool reposition,
                  bool animate,
                  bool use_spawn_animation = false);

  // Removes |overview_item| from the grid. |overview_item| cannot already be
  // absent from the grid. No items are repositioned.
  //
  // Note: This function should only be called by |OverviewSession::RemoveItem|
  // and |OverviewGrid::Shutdown|. |overview_session_| keeps count of all
  // overview items, but this function does not update the tally. If
  // |item_destroying| is true, we may want to notify |overview_session_| that
  // there are no longer any items. Calls |PositionWindows| to animate the items
  // to their new locations if |reposition| is true.
  void RemoveItem(OverviewItem* overview_item,
                  bool item_destroying = false,
                  bool reposition = false);

  // Adds a drop target for |dragged_item|, at the index immediately following
  // |dragged_item|. Repositions all items except |dragged_item|, so that the
  // drop target takes the place of |dragged_item|. Does not animate the
  // repositioning or fade in the drop target. The visual effect is that the
  // drop target was already present but was covered by |dragged_item|.
  void AddDropTargetForDraggingFromOverview(OverviewItem* dragged_item);

  // Removes the drop target from the grid.
  void RemoveDropTarget();

  // Sets bounds for the window grid and positions all windows in the grid,
  // except windows in |ignored_items|.
  void SetBoundsAndUpdatePositions(
      const gfx::Rect& bounds_in_screen,
      const base::flat_set<OverviewItem*>& ignored_items,
      bool animate);

  // Updates overview bounds and hides the drop target when a preview area is
  // shown.
  void RearrangeDuringDrag(
      aura::Window* dragged_window,
      SplitViewDragIndicators::WindowDraggingState window_dragging_state);

  // Sets the dragged window on |split_view_drag_indicators_|.
  void SetSplitViewDragIndicatorsDraggedWindow(aura::Window* dragged_window);

  // Sets the window dragging state on |split_view_drag_indicators_|.
  void SetSplitViewDragIndicatorsWindowDraggingState(
      SplitViewDragIndicators::WindowDraggingState window_dragging_state);

  // Updates the desks bar widget bounds if necessary.
  // Returns true if the desks widget's bounds have been updated.
  bool MaybeUpdateDesksWidgetBounds();

  // Updates the appearance of the drop target to visually indicate when the
  // dragged window is being dragged over it. For dragging from the top, pass
  // null for |dragged_item|.
  void UpdateDropTargetBackgroundVisibility(
      OverviewItem* dragged_item,
      const gfx::PointF& location_in_screen);

  void UpdateCannotSnapWarningVisibility();

  // Called when any OverviewItem on any OverviewGrid has started/ended being
  // dragged.
  void OnSelectorItemDragStarted(OverviewItem* item);
  void OnSelectorItemDragEnded(bool snap);

  // Called when a window (either it's browser window or an app window)
  // start/continue/end being dragged in tablet mode.
  void OnWindowDragStarted(aura::Window* dragged_window, bool animate);
  void OnWindowDragContinued(
      aura::Window* dragged_window,
      const gfx::PointF& location_in_screen,
      SplitViewDragIndicators::WindowDraggingState window_dragging_state);
  void OnWindowDragEnded(aura::Window* dragged_window,
                         const gfx::PointF& location_in_screen,
                         bool should_drop_window_into_overview,
                         bool snap);
  // Shows/Hides windows during window dragging. Used when swiping up a window
  // from shelf.
  void SetVisibleDuringWindowDragging(bool visible, bool animate);

  // Returns true if |window| is the placeholder window from the drop target.
  bool IsDropTargetWindow(aura::Window* window) const;

  // Returns the overview item that accociates with |drop_target_widget_|.
  // Returns nullptr if overview does not have the drop target.
  OverviewItem* GetDropTarget();

  // Called by |OverviewSession::OnDisplayMetricsChanged|, only for the display
  // with this grid.
  void OnDisplayMetricsChanged();

  // SplitViewObserver:
  void OnSplitViewStateChanged(SplitViewController::State previous_state,
                               SplitViewController::State state) override;
  void OnSplitViewDividerPositionChanged() override;

  // ScreenRotationAnimatorObserver:
  void OnScreenCopiedBeforeRotation() override;
  void OnScreenRotationAnimationFinished(ScreenRotationAnimator* animator,
                                         bool canceled) override;

  // WallpaperControllerObserver:
  void OnWallpaperChanging() override;
  void OnWallpaperChanged() override;

  // Called when overview starting animation completes.
  void OnStartingAnimationComplete(bool canceled);

  // Checks if the grid needs to have the wallpaper animated. Returns false if
  // one of the grids windows covers the the entire workspace, true otherwise.
  bool ShouldAnimateWallpaper() const;

  // Calculates |should_animate_when_entering_| and
  // |should_animate_when_exiting_| of the overview items based on where
  // the first MRU window covering the available workspace is found.
  // |selected_item| is not nullptr if |selected_item| is the selected item when
  // exiting overview mode. |target_bounds| are the bounds that the items will
  // be in overview. If |tranisition| is exit, |target_bounds| should be empty
  // and the overview bounds should be queried from |window_list_|.
  void CalculateWindowListAnimationStates(
      OverviewItem* selected_item,
      OverviewSession::OverviewTransition transition,
      const std::vector<gfx::RectF>& target_bounds);

  // Do not animate the entire window list during exiting the overview. It's
  // used when splitview and overview mode are both active, selecting a window
  // will put the window in splitview mode and also end the overview mode. In
  // this case the windows in OverviewGrid should not animate when exiting the
  // overivew mode. These windows will use ZERO tween so that transforms will
  // reset at the end of animation.
  void SetWindowListNotAnimatedWhenExiting();

  // Starts a nudge, with |item| being the item that may be deleted. This method
  // calculates which items in |window_list_| are to be updated, and their
  // destination bounds and fills |nudge_data_| accordingly.
  void StartNudge(OverviewItem* item);

  // Moves items in |nudge_data_| towards their destination bounds based on
  // |value|, which must be between 0.0 and 1.0.
  void UpdateNudge(OverviewItem* item, double value);

  // Clears |nudge_data_|.
  void EndNudge();

  // Called after PositionWindows when entering overview from the home launcher
  // screen. Translates all windows vertically and animates to their final
  // locations.
  void SlideWindowsIn();

  // Update the y position and opacity of the entire grid. Does this by
  // transforming the windows in |window_list_|. If |callback| is non null, the
  // transformation and opacity change should be animated. The animation
  // settings will be set by the caller via |callback|. Returns the settings of
  // the first window we are animating; the caller will observe this animation.
  // The returned object may be nullptr.
  std::unique_ptr<ui::ScopedLayerAnimationSettings> UpdateYPositionAndOpacity(
      int new_y,
      float opacity,
      OverviewSession::UpdateAnimationSettingsCallback callback);

  // Returns the window of the overview item that contains |location_in_screen|.
  // |ignored_item| is excluded from consideration. Overview items covered by
  // |ignored_item| are eligible.
  aura::Window* GetTargetWindowOnLocation(const gfx::PointF& location_in_screen,
                                          OverviewItem* ignored_item);

  // Returns true when the desks bar view is showing desks mini views (or will
  // show them once it is created).
  bool IsDesksBarViewActive() const;

  // Gets the effective bounds of this grid (the area in which the windows are
  // positioned, taking into account the availability of the Desks bar).
  gfx::Rect GetGridEffectiveBounds() const;

  // Called when a window is being dragged in Overview Mode. If
  // |update_desks_bar_drag_details| is true, it will update the drag details
  // (screen_location, and whether that location intersects with the
  // desks bar widget). |for_drop| should be set to true if this is called when
  // the item is being dropped when the drag is complete.
  // Returns true if |screen_location| does intersect with the DesksBarView.
  bool IntersectsWithDesksBar(const gfx::Point& screen_location,
                              bool update_desks_bar_drag_details,
                              bool for_drop);

  // Updates the drag details for DesksBarView to end the drag and move the
  // window of |drag_item| to another desk if it was dropped on a mini_view of
  // a desk that is different than that of the active desk.
  // Returns true if the window was successfully moved to another desk.
  bool MaybeDropItemOnDeskMiniView(const gfx::Point& screen_location,
                                   OverviewItem* drag_item);

  // Prepares the |scroll_offset_min_| as a limit for |scroll_offset| from
  // scrolling or positioning windows too far offscreen.
  void StartScroll();

  // |delta| is used for updating |scroll_offset_| with new scroll values so
  // that windows in tablet overview mode get positioned accordingly. Returns
  // true if the grid was moved to the edge.
  bool UpdateScrollOffset(float delta);

  void EndScroll();

  // Calculate the width of an item based on |height|. The width tries to keep
  // the same aspect ratio as the original window, but may be modified if the
  // bounds of the window are considered extreme, or if the window is in
  // splitview or entering splitview.
  int CalculateWidthAndMaybeSetUnclippedBounds(OverviewItem* item, int height);

  // Called when a desk is added or removed to update the bounds of the desks
  // widget as it may need to switch between default and compact layouts.
  void OnDesksChanged();

  // Returns true if the grid has no more windows.
  bool empty() const { return window_list_.empty(); }

  // Returns how many overview items are in the grid.
  size_t size() const { return window_list_.size(); }

  // Returns the root window in which the grid displays the windows.
  const aura::Window* root_window() const { return root_window_; }

  OverviewSession* overview_session() { return overview_session_; }

  const std::vector<std::unique_ptr<OverviewItem>>& window_list() const {
    return window_list_;
  }

  SplitViewDragIndicators* split_view_drag_indicators() {
    return split_view_drag_indicators_.get();
  }

  const DesksBarView* desks_bar_view() const { return desks_bar_view_; }

  const gfx::Rect bounds() const { return bounds_; }

  bool should_animate_when_exiting() const {
    return should_animate_when_exiting_;
  }

  void set_suspend_reposition(bool value) { suspend_reposition_ = value; }

  views::Widget* drop_target_widget() { return drop_target_widget_.get(); }

  float scroll_offset() const { return scroll_offset_; }

  OverviewGridEventHandler* grid_event_handler() {
    return grid_event_handler_.get();
  }

 private:
  class TargetWindowObserver;
  friend class OverviewSessionTest;

  // Struct which holds data required to perform nudges.
  struct NudgeData {
    size_t index;
    gfx::RectF src;
    gfx::RectF dst;
  };

  // If the Virtual Desks feature is enabled, it initializes the widget that
  // contains the DeskBarView contents.
  void MaybeInitDesksWidget();

  // Gets the layout of the overview items. Layout is done in 2 stages
  // maintaining fixed MRU ordering.
  // 1. Optimal height is determined. In this stage |height| is bisected to find
  //    maximum height which still allows all the windows to fit.
  // 2. Row widths are balanced. In this stage the available width is reduced
  //    until some windows are no longer fitting or until the difference between
  //    the narrowest and the widest rows starts growing.
  // Overall this achieves the goals of maximum size for previews (or maximum
  // row height which is equivalent assuming fixed height), balanced rows and
  // minimal wasted space.
  std::vector<gfx::RectF> GetWindowRects(
      const base::flat_set<OverviewItem*>& ignored_items);

  // Gets the layout of the overview items. Currently only for tablet mode.
  // Positions up to six windows into two rows of equal height, scaling each
  // window to fit that height. Additional windows are placed off-screen.
  // |ignored_items| won't be shown along with the other windows in overview
  // mode.
  std::vector<gfx::RectF> GetWindowRectsForTabletModeLayout(
      const base::flat_set<OverviewItem*>& ignored_items);

  // Attempts to fit all |out_rects| inside |bounds|. The method ensures that
  // the |out_rects| vector has appropriate size and populates it with the
  // values placing rects next to each other left-to-right in rows of equal
  // |height|. While fitting |out_rects| several metrics are collected that can
  // be used by the caller. |out_max_bottom| specifies the bottom that the rects
  // are extending to. |out_min_right| and |out_max_right| report the right
  // bound of the narrowest and the widest rows respectively. In-values of the
  // |out_max_bottom|, |out_min_right| and |out_max_right| parameters are
  // ignored and their values are always initialized inside this method. Returns
  // true on success and false otherwise.
  bool FitWindowRectsInBounds(
      const gfx::Rect& bounds,
      int height,
      const base::flat_set<OverviewItem*>& ignored_items,
      std::vector<gfx::RectF>* out_rects,
      int* out_max_bottom,
      int* out_min_right,
      int* out_max_right);

  // Returns the overview item iterator that contains |window|.
  std::vector<std::unique_ptr<OverviewItem>>::iterator
  GetOverviewItemIterContainingWindow(aura::Window* window);

  // Returns the index of |item| in |window_list_|.
  size_t GetOverviewItemIndex(OverviewItem* item) const;

  // Adds the |dragged_window| into overview on drag ended. Might need to update
  // the window's bounds if it has been resized.
  void AddDraggedWindowIntoOverviewOnDragEnd(aura::Window* dragged_window);

  // Returns the the bounds of the desks widget in root window.
  gfx::Rect GetDesksWidgetBounds() const;

  // Root window the grid is in.
  aura::Window* root_window_;

  // Pointer to the OverviewSession that spawned this grid.
  OverviewSession* overview_session_;

  // Vector containing all the windows in this grid.
  std::vector<std::unique_ptr<OverviewItem>> window_list_;

  // The owner of the widget that displays split-view-related information. Null
  // if split view is unsupported (see |ShouldAllowSplitView|).
  std::unique_ptr<SplitViewDragIndicators> split_view_drag_indicators_;

  // Widget that contains the DeskBarView contents when the Virtual Desks
  // feature is enabled.
  std::unique_ptr<views::Widget> desks_widget_;
  // The contents view of the above |desks_widget_| if created.
  DesksBarView* desks_bar_view_ = nullptr;

  // The drop target widget. The drop target is created when a window or
  // overview item is being dragged, and is destroyed when the drag ends or
  // overview mode is ended. The drop target is hidden when a snap preview area
  // is shown. You can drop a window into overview by dragging to the drop
  // target or by dragging to almost anywhere while the drop target is shown. A
  // plus sign in the center of the drop target indicates tab dragging.
  std::unique_ptr<views::Widget> drop_target_widget_;

  // The observer of the target window, which is the window that the dragged
  // tabs are going to merge into after the drag ends. After the dragged tabs
  // merge into the target window, and if the target window is a minimized
  // window in overview and is not destroyed yet, we need to update the overview
  // minimized widget's content view so that it reflects the merge.
  std::unique_ptr<TargetWindowObserver> target_window_observer_;

  // True if the overview grid should animate when exiting overview mode. Note
  // even if it's true, it doesn't mean all window items in the grid should
  // animate when exiting overview, instead each window item's animation status
  // is controlled by its own |should_animate_when_exiting_|. But if it's false,
  // all window items in the grid don't have animation.
  bool should_animate_when_exiting_ = true;

  // This OverviewGrid's total bounds in screen coordinates.
  gfx::Rect bounds_;

  // Collection of the items which should be nudged. This should only be
  // non-empty if a nudge is in progress.
  std::vector<NudgeData> nudge_data_;

  // Measures the animation smoothness of overview animation.
  std::unique_ptr<FpsCounter> fps_counter_;

  // True to skip |PositionWindows()|. Used to avoid O(n^2) layout when
  // reposition windows in tablet overview mode.
  bool suspend_reposition_ = false;

  // Used by |GetWindowRectsForTabletModeLayout| to shift the x position of the
  // overview items.
  float scroll_offset_ = 0;

  // Value to clamp |scroll_offset| so scrolling stays limited to windows that
  // are visible in tablet overview mode.
  float scroll_offset_min_ = 0;

  // Cached values of the item bounds so that they do not have to be calculated
  // on each scroll update.
  std::vector<gfx::RectF> items_scrolling_bounds_;

  // Handles events that are not handled by the OverviewItems.
  std::unique_ptr<OverviewGridEventHandler> grid_event_handler_;

  // Records the presentation time of scrolling the grid in overview mode.
  std::unique_ptr<PresentationTimeRecorder> presentation_time_recorder_;

  // Weak pointer to the window that is being dragged from the top, if there is
  // one.
  aura::Window* dragged_window_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(OverviewGrid);
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_OVERVIEW_GRID_H_
