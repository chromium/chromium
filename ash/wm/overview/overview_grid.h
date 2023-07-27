// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_OVERVIEW_GRID_H_
#define ASH_WM_OVERVIEW_OVERVIEW_GRID_H_

#include <stddef.h>

#include <memory>
#include <vector>

#include "ash/public/cpp/wallpaper/wallpaper_controller_observer.h"
#include "ash/rotator/screen_rotation_animator_observer.h"
#include "ash/style/rounded_label_widget.h"
#include "ash/wm/desks/templates/saved_desk_save_desk_button_container.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/splitview/split_view_drag_indicators.h"
#include "ash/wm/splitview/split_view_observer.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "ui/aura/window.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"

namespace views {
class Widget;
}

namespace ui {
class PresentationTimeRecorder;
}

namespace ash {

class LegacyDeskBarView;
class OverviewGridEventHandler;
class OverviewItem;
class SavedDeskSaveDeskButton;
class SavedDeskSaveDeskButtonContainer;
class SavedDeskLibraryView;

// Manages and positions the overview UI on a per root window basis. Overview UI
// elements include:
//   - Desks bar view which contains a desk preview and desk name per desk.
//   - Splitview indicators for snapping windows in overview.
//   - Overview items representing each application window associated with the
//     root window of the grid.
//   - Saved desk UI elements to create saved desks and display saved desks.
//   - etc.
class ASH_EXPORT OverviewGrid : public SplitViewObserver,
                                public ScreenRotationAnimatorObserver,
                                public WallpaperControllerObserver {
 public:
  class MetricsTracker {
   public:
    MetricsTracker() = default;
    virtual ~MetricsTracker() = default;
  };

  OverviewGrid(aura::Window* root_window,
               const std::vector<aura::Window*>& window_list,
               OverviewSession* overview_session);

  OverviewGrid(const OverviewGrid&) = delete;
  OverviewGrid& operator=(const OverviewGrid&) = delete;

  ~OverviewGrid() override;

  // Exits overview mode.
  void Shutdown(OverviewEnterExitType exit_type);

  // Prepares the windows in this grid for overview. This will restore all
  // minimized windows and ensure they are visible.
  void PrepareForOverview();

  // Positions all the windows in rows of equal height scaling each window to
  // fit that height. Optionally animates the windows to their targets when
  // |animate| is true. Items in |ignored_items| are not positioned. This is for
  // dragging. |transition| specifies the overview state when this function is
  // called. Updates the save desk template button if necessary.
  void PositionWindows(
      bool animate,
      const base::flat_set<OverviewItem*>& ignored_items = {},
      OverviewTransition transition = OverviewTransition::kInOverview);

  // Returns the OverviewItem if a window is contained in any of the
  // OverviewItems this grid owns. Returns nullptr if no such a OverviewItem
  // exist.
  OverviewItem* GetOverviewItemContaining(const aura::Window* window) const;

  // TODO(b/285408040): Handle two finger scroll and make it smooth.
  void HandleMouseWheelScrollEvent(int scroll_offset);

  // Check if in tablet mode or the new clamshell scroll layout feature is
  // enabled. If so, the visible windows on the overview screen exceed
  // `kMinimumItemsForNewLayoutInClamshell` or
  // `kMinimumItemsForNewLayoutInTablet` thereby cluttering the overview screen.
  bool ShouldUseScrollingLayout(size_t ignored_items_size) const;

  // Adds |window| at the specified |index|. |window| cannot already be on the
  // grid. If |reposition| is true, repositions all items except those in
  // |ignored_items|. If |animate| is true, animates the repositioning.
  // |animate| has no effect if |reposition| is false.
  // If |use_spawn_animation| is true, and this item is being added *while*
  // overview is already active, it will use a special spawn animation on its
  // first position in the grid. |use_spawn_animation| has no effect if either
  // |animate| or |reposition| are false.
  // If |reposition|, |animate|, and |restack| are all true, the stacking order
  // will be adjusted after the animation. If |restack| is true but at least one
  // of |reposition| and |animate| is false, the stacking order will be adjusted
  // immediately.
  // Note: OverviewSession has versions of the Add/Remove items. Those are
  // preferred as they will call into these functions and update other things
  // like the overview accessibility annotator and the no recent items widget.
  void AddItem(aura::Window* window,
               bool reposition,
               bool animate,
               const base::flat_set<OverviewItem*>& ignored_items,
               size_t index,
               bool use_spawn_animation,
               bool restack);

  // Similar to the above function, but adds the window to the end of the grid.
  void AppendItem(aura::Window* window,
                  bool reposition,
                  bool animate,
                  bool use_spawn_animation);

  // Like |AddItem|, but adds |window| at the correct position according to MRU
  // order.
  void AddItemInMruOrder(aura::Window* window,
                         bool reposition,
                         bool animate,
                         bool restack,
                         bool use_spawn_animation);

  // Removes |overview_item| from the grid. |overview_item| cannot already be
  // absent from the grid. If |item_destroying| is true, we may want to notify
  // |overview_session_| that this grid has become empty. If |item_destroying|
  // and |reposition| are both true, all items are repositioned with animation.
  // |reposition| has no effect if |item_destroying| is false.
  void RemoveItem(OverviewItem* overview_item,
                  bool item_destroying,
                  bool reposition);

  // Removes all overview items and restores the respective windows. This is
  // used when launching a saved desk. While this will empty the grid, it will
  // *not* invoke `OverviewSession::OnGridEmpty()` since the grid is about to
  // get filled with new windows.
  void RemoveAllItemsForSavedDeskLaunch();

  // Adds a drop target for |dragged_item|, at the index immediately following
  // |dragged_item|. Repositions all items except |dragged_item|, so that the
  // drop target takes the place of |dragged_item|. Does not animate the
  // repositioning or fade in the drop target. The visual effect is that the
  // drop target was already present but was covered by |dragged_item|.
  void AddDropTargetForDraggingFromThisGrid(OverviewItem* dragged_item);

  // Adds a drop target for |dragged_window|. Used for dragging from another
  // grid, from the top in tablet mode, or from the shelf in tablet mode.
  void AddDropTargetNotForDraggingFromThisGrid(aura::Window* dragged_window,
                                               bool animate);

  // Removes the drop target from the grid.
  void RemoveDropTarget();

  // Sets bounds for the window grid and positions all windows in the grid,
  // except windows in |ignored_items|.
  void SetBoundsAndUpdatePositions(
      const gfx::Rect& bounds_in_screen,
      const base::flat_set<OverviewItem*>& ignored_items,
      bool animate);

  // Updates overview bounds and hides the drop target when a preview area is
  // shown or the drag is currently outside of |root_window_|. For dragging from
  // the top or from the shelf, pass null for |dragged_item|.
  void RearrangeDuringDrag(
      OverviewItem* dragged_item,
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
  // dragged window is being dragged over it. For dragging from the top or from
  // the shelf, pass null for |dragged_item|.
  void UpdateDropTargetBackgroundVisibility(
      OverviewItem* dragged_item,
      const gfx::PointF& location_in_screen);

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

  // Called when a WebUI Tab Strip thumbnail is dropped into overview grid.
  void MergeWindowIntoOverviewForWebUITabStrip(aura::Window* dragged_window);

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

  // Called by |OverviewSession::OnUserWorkAreaInsetsChanged|.
  void OnUserWorkAreaInsetsChanged(aura::Window* root_window);

  // Called when overview starting animation completes.
  void OnStartingAnimationComplete(bool canceled);

  // Calculates |should_animate_when_entering_| and
  // |should_animate_when_exiting_| of the overview items based on where
  // the first MRU window covering the available workspace is found.
  // |selected_item| is not nullptr if |selected_item| is the selected item when
  // exiting overview mode. |target_bounds| are the bounds that the items will
  // be in overview. If |tranisition| is exit, |target_bounds| should be empty
  // and the overview bounds should be queried from |window_list_|.
  void CalculateWindowListAnimationStates(
      OverviewItem* selected_item,
      OverviewTransition transition,
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

  // Gets the insets of the grid. Either |bounds_| or GetGridEffectiveBounds
  // does not exclude the insets from its bounds. But like PositionWindows needs
  // to position the overview windows in the bounds exclude the insets.
  gfx::Insets GetGridInsets() const;

  // Called when a window is being dragged in Overview Mode. If
  // |update_desks_bar_drag_details| is true, it will update the drag details
  // (screen_location, and whether that location intersects with the
  // desks bar widget). |for_drop| should be set to true if this is called when
  // the item is being dropped when the drag is complete.
  // Returns true if |screen_location| does intersect with the
  // LegacyDeskBarView.
  bool IntersectsWithDesksBar(const gfx::Point& screen_location,
                              bool update_desks_bar_drag_details,
                              bool for_drop);

  // Updates the drag details for LegacyDeskBarView to end the drag and move the
  // window of |drag_item| to another desk if it was dropped on a mini_view of
  // a desk that is different than that of the active desk or if dropped on the
  // new desk button. Returns true if the window was successfully moved to
  // another desk.
  bool MaybeDropItemOnDeskMiniViewOrNewDeskButton(
      const gfx::Point& screen_location,
      OverviewItem* drag_item);

  // Transforms `desks_bar_view_` from zero state to expanded state. Called when
  // a normal drag starts to enable user dragging a window and dropping it to
  // the new desk. `screen_location` is the center point of the window being
  // dragged.
  void MaybeExpandDesksBarView(const gfx::PointF& screen_location);

  // Transforms `desks_bar_view_` from expanded state to zero state. Called when
  // a normal drag is completed.
  void MaybeShrinkDesksBarView();

  // Prepares the |scroll_offset_min_| as a limit for |scroll_offset| from
  // scrolling or positioning windows too far offscreen.
  void StartScroll();

  // |delta| is used for updating |scroll_offset_| with new scroll values so
  // that windows in tablet overview mode get positioned accordingly. Returns
  // true if the grid was moved to the edge.
  bool UpdateScrollOffset(float delta);

  void EndScroll();

  // Calculates the width of an item based on |height|. The width tries to keep
  // the same aspect ratio as the original window, but may be modified if the
  // bounds of the window are considered extreme, or if the window is in
  // splitview or entering splitview.
  int CalculateWidthAndMaybeSetUnclippedBounds(OverviewItem* item, int height);

  // Returns true if any desk name is being modified in its mini view on this
  // grid.
  bool IsDeskNameBeingModified() const;

  // Commits any on-going name changes if any.
  void CommitNameChanges();

  // Shows the saved desk library. Creates the widget if needed. The desks bar
  // will be expanded if it isn't already.
  void ShowSavedDeskLibrary();

  // Hides the saved desk library and reshows the overview items. Updates the
  // save desk buttons if we are not exiting overview.
  void HideSavedDeskLibrary(bool exit_overview);

  // True if the saved desk library is shown, or in the process of animating to
  // be shown.
  bool IsShowingSavedDeskLibrary() const;

  // Returns true if any saved desk name is being modified in its item view on
  // this grid.
  bool IsSavedDeskNameBeingModified() const;

  // Updates the visibility of the `no_windows_widget_`. If `no_items` is true,
  // the widget will be shown. If `no_items` is false or the desk templates grid
  // is visible, the widget will be hidden.
  void UpdateNoWindowsWidget(bool no_items);

  // Refreshes the bounds of `no_windows_widget_`, animating if `animate` is
  // true.
  void RefreshNoWindowsWidgetBounds(bool animate);

  // Updates bounds, tooltips and a11y focus, as well as handles animations on
  // `save_desk_button_container_widget_`.
  void UpdateSaveDeskButtons();

  // Enable the save desk button container.
  void EnableSaveDeskButtonContainer();

  bool IsSaveDeskButtonContainerVisible() const;
  bool IsSaveDeskAsTemplateButtonVisible() const;
  bool IsSaveDeskForLaterButtonVisible() const;

  // Returns the save desk as template button if available, otherwise null.
  SavedDeskSaveDeskButton* GetSaveDeskAsTemplateButton() const;

  // Returns the save desk for later button if available, otherwise null.
  SavedDeskSaveDeskButton* GetSaveDeskForLaterButton() const;

  // Returns the save button container if available, otherwise null.
  SavedDeskSaveDeskButtonContainer* GetSaveDeskButtonContainer() const;

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

  // Returns the saved desk library view, or nullptr.
  SavedDeskLibraryView* GetSavedDeskLibraryView() const;

  // Returns true if the grid has no more windows.
  bool empty() const { return window_list_.empty(); }

  // Returns how many overview items are in the grid.
  size_t size() const { return window_list_.size(); }

  // Returns the root window in which the grid displays the windows.
  aura::Window* root_window() { return root_window_; }

  OverviewSession* overview_session() { return overview_session_; }

  const std::vector<std::unique_ptr<OverviewItem>>& window_list() const {
    return window_list_;
  }

  RoundedLabelWidget* no_windows_widget() { return no_windows_widget_.get(); }

  SplitViewDragIndicators* split_view_drag_indicators() {
    return split_view_drag_indicators_.get();
  }

  const views::Widget* desks_widget() const { return desks_widget_.get(); }

  const LegacyDeskBarView* desks_bar_view() const { return desks_bar_view_; }
  LegacyDeskBarView* desks_bar_view() { return desks_bar_view_; }

  bool should_animate_when_exiting() const {
    return should_animate_when_exiting_;
  }

  void set_suspend_reposition(bool value) { suspend_reposition_ = value; }

  views::Widget* drop_target_widget() { return drop_target_widget_.get(); }

  OverviewGridEventHandler* grid_event_handler() {
    return grid_event_handler_.get();
  }

  views::Widget* saved_desk_library_widget() const {
    return saved_desk_library_widget_.get();
  }

  views::Widget* save_desk_button_container_widget() const {
    return save_desk_button_container_widget_.get();
  }

  int num_incognito_windows() const { return num_incognito_windows_; }

  int num_unsupported_windows() const { return num_unsupported_windows_; }

  const gfx::Rect bounds_for_testing() const { return bounds_; }
  float scroll_offset_for_testing() const { return scroll_offset_; }

 private:
  friend class DesksTemplatesTest;
  friend class OverviewTestBase;

  // Struct which holds data required to perform nudges. Nudge in the context of
  // overview view means an overview item is currently being dragged vertically
  // and may be closed when released, and the remaining windows will move
  // towards their positions once the item is closed.
  // TODO(conniekxu|sammiequon): Rename this as nudge has a different name in
  // cros system UI.
  struct NudgeData {
    size_t index;
    gfx::RectF src;
    gfx::RectF dst;
  };

  // Initializes the widget that contains the `LegacyDeskBarView` contents. Also
  // will update the save desk buttons visibility after we initialize
  // `LegacyDeskBarView`.
  void MaybeInitDesksWidget();

  // Gets the layout of the overview items. Layout is done in 2 stages
  // maintaining fixed MRU ordering.
  // 1. Optimal height is determined. In this stage `height` is bisected to find
  //    maximum height which still allows all the windows to fit.
  // 2. Row widths are balanced. In this stage the available width is reduced
  //    until some windows are no longer fitting or until the difference between
  //    the narrowest and the widest rows starts growing.
  // Overall this achieves the goals of maximum size for previews (or maximum
  // row height which is equivalent assuming fixed height), balanced rows and
  // minimal wasted space.
  std::vector<gfx::RectF> GetWindowRects(
      const base::flat_set<OverviewItem*>& ignored_items);

  // Gets the layout of the overview items. Positions up to six windows into
  // two rows of equal height, scaling each window to fit that height.
  // Additional windows are placed off-screen. |ignored_items| won't be shown
  // along with the other windows in overview mode. If
  // `IsOverviewScrollLayoutForClamshellEnabled`, then the behavior is
  // replicated but in the vertical direction for clamshell mode.
  // TODO(b/286869951): Reduce duplication once clamshell scrolling is
  // finalized.
  std::vector<gfx::RectF> GetWindowRectsForScrollingLayout(
      const base::flat_set<OverviewItem*>& ignored_items);

  std::vector<gfx::RectF> GetRectsForClamshellScroll(
      const base::flat_set<OverviewItem*>& ignored_items);

  std::vector<gfx::RectF> GetRectsForTabletScroll(
      const base::flat_set<OverviewItem*>& ignored_items);

  // Attempts to fit all `out_rects` inside `bounds`. The method ensures that
  // the `out_rects` vector has appropriate size and populates it with the
  // values placing rects next to each other left-to-right in rows of equal
  // `height`. While fitting `out_rects` several metrics are collected that can
  // be used by the caller. `out_max_bottom` specifies the bottom that the rects
  // are extending to. `out_min_right` and `out_max_right` report the right
  // bound of the narrowest and the widest rows respectively. In-values of the
  // `out_max_bottom`, `out_min_right` and `out_max_right` parameters are
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

  // Returns the index of |item| in |window_list_|.
  size_t GetOverviewItemIndex(OverviewItem* item) const;

  // Returns the index where |window| can be inserted into |window_list_| based
  // on MRU order.
  size_t FindInsertionIndex(const aura::Window* window);

  // Adds the |dragged_window| into overview on drag ended. Might need to update
  // the window's bounds if it has been resized.
  void AddDraggedWindowIntoOverviewOnDragEnd(aura::Window* dragged_window);

  // Returns the the bounds of the desks widget in screen coordinates.
  gfx::Rect GetDesksWidgetBounds() const;

  void UpdateCannotSnapWarningVisibility(bool animate);

  // Called back when the button to save desk as template button is pressed.
  void OnSaveDeskAsTemplateButtonPressed();

  // Called back when the button to save a desk for later is pressed.
  void OnSaveDeskForLaterButtonPressed();

  // Called when the animation for fading the `saved_desk_grid_widget_` out is
  // completed.
  void OnSavedDeskGridFadedOut();

  // Called when the animation for fading the
  // `save_desk_button_container_widget_` out is completed.
  void OnSaveDeskButtonContainerFadedOut();

  // Updates the number of unsupported windows of saved desk. This includes
  // `num_incognito_windows_` and `num_unsupported_windows` as of now. When
  // `window` is being added to the grid, `increment` is true, and false
  // otherwise.
  void UpdateNumSavedDeskUnsupportedWindows(aura::Window* window,
                                            bool increment);

  // Returns the height of `desks_bar_view_`.
  int GetDesksBarHeight() const;

  // Root window the grid is in.
  raw_ptr<aura::Window, ExperimentalAsh> root_window_;

  // Pointer to the OverviewSession that spawned this grid.
  raw_ptr<OverviewSession, ExperimentalAsh> overview_session_;

  // Vector containing all the windows in this grid.
  std::vector<std::unique_ptr<OverviewItem>> window_list_;

  // A widget that is shown if we entered overview without any windows opened.
  std::unique_ptr<RoundedLabelWidget> no_windows_widget_;

  // The owner of the widget that displays split-view-related information. Null
  // if split view is unsupported (see |ShouldAllowSplitView|).
  std::unique_ptr<SplitViewDragIndicators> split_view_drag_indicators_;

  // Widget that contains the DeskBarView contents when the Virtual Desks
  // feature is enabled.
  std::unique_ptr<views::Widget> desks_widget_;
  // The contents view of the above |desks_widget_| if created.
  raw_ptr<LegacyDeskBarView, ExperimentalAsh> desks_bar_view_ = nullptr;

  // The drop target widget. The drop target is created when a window or
  // overview item is being dragged, and is destroyed when the drag ends or
  // overview mode is ended. The drop target is hidden when a snap preview area
  // is shown. You can drop a window into overview by dragging to the drop
  // target or by dragging to almost anywhere while the drop target is shown.
  std::unique_ptr<views::Widget> drop_target_widget_;

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
  std::unique_ptr<MetricsTracker> metrics_tracker_;

  // True to skip |PositionWindows()|. Used to avoid O(n^2) layout when
  // reposition windows in tablet overview mode.
  bool suspend_reposition_ = false;

  // Used by `GetWindowRectsForScrollingLayout` to shift the x position of the
  // overview items and y position if
  // `IsOverviewScrollLayoutForClamshellEnabled`.
  float scroll_offset_ = 0;

  // Value to clamp `scroll_offset` so scrolling stays limited to windows that
  // are visible in the new scrolling layout for overview mode.
  float scroll_offset_min_ = 0.f;

  // Handles events that are not handled by the OverviewItems.
  std::unique_ptr<OverviewGridEventHandler> grid_event_handler_;

  // Records the presentation time of scrolling the grid in overview mode.
  std::unique_ptr<ui::PresentationTimeRecorder> presentation_time_recorder_;

  // Weak pointer to the window that is being dragged from the top, if there is
  // one.
  raw_ptr<aura::Window, ExperimentalAsh> dragged_window_ = nullptr;

  // The widget that contains the view for all saved desks.
  std::unique_ptr<views::Widget> saved_desk_library_widget_;

  // A widget that contains save desk buttons which save desk as template or for
  // later when pressed.
  std::unique_ptr<views::Widget> save_desk_button_container_widget_;

  // The number of incognito windows in this grid. Used by saved desks to
  // identify the unsupported window type to the user.
  int num_incognito_windows_ = 0;

  // The number of unsupported windows in this grid. Used by saved desks to
  // identify the unsupported window type to the user.
  int num_unsupported_windows_ = 0;

  base::WeakPtrFactory<OverviewGrid> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_OVERVIEW_GRID_H_
