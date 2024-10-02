// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_OVERVIEW_GRID_H_
#define ASH_WM_OVERVIEW_OVERVIEW_GRID_H_

#include <memory>
#include <vector>

#include "ash/public/cpp/desk_template.h"
#include "ash/public/cpp/wallpaper/wallpaper_controller_observer.h"
#include "ash/rotator/screen_rotation_animator_observer.h"
#include "ash/wm/desks/desk_bar_view_base.h"
#include "ash/wm/desks/templates/saved_desk_util.h"
#include "ash/wm/overview/birch/birch_bar_view.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_item.h"
#include "ash/wm/overview/overview_observer.h"
#include "ash/wm/overview/overview_types.h"
#include "ash/wm/overview/overview_ui_task_pool.h"
#include "ash/wm/splitview/split_view_drag_indicators.h"
#include "ash/wm/splitview/split_view_observer.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"

namespace aura {
class Window;
}  // namespace aura

namespace gfx {
class Rect;
class RectF;
}  // namespace gfx

namespace views {
class Widget;
}  // namespace views

namespace ui {
class PresentationTimeRecorder;
}  // namespace ui

namespace ash {

class OverviewDeskBarView;
class OverviewDropTarget;
class OverviewGridEventHandler;
class OverviewItemBase;
class OverviewSession;
class RoundedLabelWidget;
class SavedDeskSaveDeskButton;
class SavedDeskSaveDeskButtonContainer;
class SavedDeskLibraryView;
class ScopedOverviewHideWindows;
class ScopedOverviewWallpaperClipper;
class SplitViewController;
class SplitViewSetupView;
class WindowOcclusionCalculator;

// An instance of this class is created during the initialization of an overview
// session which manages and positions the overview UI on a per root window
// basis. Overview UI elements include:
//   - Desks bar view which contains a desk preview and desk name per desk.
//   - Splitview indicators for snapping windows in overview.
//   - Overview items representing each application window associated with the
//     root window of the grid.
//   - Saved desk UI elements to create saved desks and display saved desks.
//   - etc.
class ASH_EXPORT OverviewGrid : public SplitViewObserver,
                                public ScreenRotationAnimatorObserver,
                                public WallpaperControllerObserver,
                                public OverviewItem::WindowDestructionDelegate {
 public:
  class MetricsTracker {
   public:
    MetricsTracker() = default;
    virtual ~MetricsTracker() = default;
  };

  OverviewGrid(
      aura::Window* root_window,
      const std::vector<raw_ptr<aura::Window, VectorExperimental>>& window_list,
      OverviewSession* overview_session,
      base::WeakPtr<WindowOcclusionCalculator> window_occlusion_calculator);

  OverviewGrid(const OverviewGrid&) = delete;
  OverviewGrid& operator=(const OverviewGrid&) = delete;

  ~OverviewGrid() override;

  const gfx::Rect& bounds() const { return bounds_; }

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
      const base::flat_set<OverviewItemBase*>& ignored_items = {},
      OverviewTransition transition = OverviewTransition::kInOverview);

  // Used when feature ContinuousOverviewScrollAnimation is enabled. Positions
  // the windows according to the y_offset. Uses the same logic as
  // `PositionWindows()` to determine the final state of each window. Minimized
  // windows, and the save desk button, fade in accordingly based on the scroll
  // offset.
  void PositionWindowsContinuously(float y_offset);

  // Returns the `OverviewItemBase` if a window is contained in any of the
  // OverviewItems this grid owns. Returns nullptr if no such a OverviewItem
  // exist.
  OverviewItemBase* GetOverviewItemContaining(const aura::Window* window) const;

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
               const base::flat_set<OverviewItemBase*>& ignored_items,
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
  void RemoveItem(OverviewItemBase* overview_item,
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
  void AddDropTargetForDraggingFromThisGrid(OverviewItemBase* dragged_item);

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
      const base::flat_set<OverviewItemBase*>& ignored_items,
      bool animate);

  // Updates overview bounds and hides the drop target when a preview area is
  // shown or the drag is currently outside of |root_window_|. For dragging from
  // the top or from the shelf, pass null for |dragged_item|.
  void RearrangeDuringDrag(
      OverviewItemBase* dragged_item,
      SplitViewDragIndicators::WindowDraggingState window_dragging_state);

  // Sets the dragged window on |split_view_drag_indicators_|.
  void SetSplitViewDragIndicatorsDraggedWindow(aura::Window* dragged_window);

  // Sets the window dragging state on |split_view_drag_indicators_|.
  void SetSplitViewDragIndicatorsWindowDraggingState(
      SplitViewDragIndicators::WindowDraggingState window_dragging_state);

  // Updates the desks bar widget bounds if necessary.
  // Returns true if the desks widget's bounds have been updated.
  bool MaybeUpdateDesksWidgetBounds();

  // Updates the birch bar widget bounds if necessary.
  // Returns true if the birch bar widget's bounds have been updated.
  bool MaybeUpdateBirchBarWidgetBounds();

  // Updates the appearance of the drop target to visually indicate when the
  // dragged window is being dragged over it. For dragging from the top or from
  // the shelf, pass null for |dragged_item|.
  void UpdateDropTargetBackgroundVisibility(
      OverviewItemBase* dragged_item,
      const gfx::PointF& location_in_screen);

  // Called when any OverviewItem on any OverviewGrid has started/ended being
  // dragged.
  void OnOverviewItemDragStarted();
  void OnOverviewItemDragEnded(bool snap);

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

  // Called by |OverviewSession::OnDisplayMetricsChanged|, only for the display
  // with this grid.
  void OnDisplayMetricsChanged(uint32_t changed_metrics);

  // Called by |OverviewSession::OnUserWorkAreaInsetsChanged|.
  void OnUserWorkAreaInsetsChanged(aura::Window* root_window);

  // Called when overview starting animation completes.
  void OnStartingAnimationComplete(bool canceled);

  // Calculates `should_animate_when_entering_` and
  // `should_animate_when_exiting_` of overview items. This to animate only what
  // is necessary for performance reasons. A window will not be animated if
  // both its source and target bounds are covered by another window higher in
  // z-order. For example, if the MRU window is maximized and we have no floated
  // or always on top windows, that window will be the only animated window
  // entering or exiting overview. `selected_item` is the selected item when
  // exiting overview mode, null otherwise. `target_bounds` are the bounds that
  // the items will be in overview. If `tranisition` is exit, `target_bounds`
  // should be empty and the overview bounds should be queried from
  // `item_list_`.
  void CalculateWindowListAnimationStates(
      OverviewItemBase* selected_item,
      OverviewTransition transition,
      const std::vector<gfx::RectF>& target_bounds);

  // Do not animate the entire window list during exiting the overview. It's
  // used when splitview and overview mode are both active, selecting a window
  // will put the window in splitview mode and also end the overview mode. In
  // this case the windows in OverviewGrid should not animate when exiting the
  // overivew mode. These windows will use ZERO tween so that transforms will
  // reset at the end of animation.
  void SetWindowListNotAnimatedWhenExiting();

  // Starts a nudge, with `item` being the item that may be deleted. This method
  // calculates which items in `item_list_` are to be updated, and their
  // destination bounds and fills `nudge_data_` accordingly.
  void StartNudge(OverviewItemBase* item);

  // Moves items in |nudge_data_| towards their destination bounds based on
  // |value|, which must be between 0.0 and 1.0.
  void UpdateNudge(OverviewItemBase* item, double value);

  // Clears |nudge_data_|.
  void EndNudge();

  // Returns the window of the overview item that contains |location_in_screen|.
  // |ignored_item| is excluded from consideration. Overview items covered by
  // |ignored_item| are eligible.
  aura::Window* GetTargetWindowOnLocation(const gfx::PointF& location_in_screen,
                                          OverviewItemBase* ignored_item);

  // Returns true when the desks bar view is showing desks mini views (or will
  // show them once it is created).
  bool IsDesksBarViewActive() const;

  // Gets the effective bounds of this grid (the area in which the windows are
  // positioned, taking into account the availability of the desks bar and birch
  // bar).
  gfx::Rect GetGridEffectiveBounds() const;

  // Gets the horizontal paddings according to the shelf alignment and the
  // existence of split view.
  gfx::Insets GetGridHorizontalPaddings() const;

  // Gets the vertical paddings according to the existence of desk bar, birch
  // bar, shelf and split view.
  gfx::Insets GetGridVerticalPaddings() const;

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
  // OverviewDeskBarView.
  bool IntersectsWithDesksBar(const gfx::Point& screen_location,
                              bool update_desks_bar_drag_details,
                              bool for_drop);

  // Updates the drag details for OverviewDeskBarView to end the drag and move
  // the window(s) represented by the `dragged_item` to another desk if it was
  // dropped on a mini_view of a desk that is different than that of the active
  // desk or if dropped on the new desk button. Returns true if the window(s)
  // were successfully moved to another desk.
  bool MaybeDropItemOnDeskMiniViewOrNewDeskButton(
      const gfx::Point& screen_location,
      OverviewItemBase* dragged_item);

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
  int CalculateWidthAndMaybeSetUnclippedBounds(OverviewItemBase* item,
                                               int height);

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
  void UpdateNoWindowsWidget(bool no_items,
                             bool animate,
                             bool is_continuous_enter);

  // Refreshes this grid's bounds. This will set bounds and update the overview
  // item positions depending on the current split view state.
  void RefreshGridBounds(bool animate);

  // Updates bounds, tooltips and a11y focus, as well as handles animations on
  // `save_desk_button_container_widget_`.
  void UpdateSaveDeskButtons();

  // Enable the save desk button container.
  void EnableSaveDeskButtonContainer();

  bool IsSaveDeskButtonContainerVisible() const;
  bool IsSaveDeskAsTemplateButtonVisible() const;
  bool IsSaveDeskForLaterButtonVisible() const;

  // Called by `OverviewSession` when tablet mode changes to update necessary UI
  // if needed.
  void OnTabletModeChanged();

  // This is different from `item_list_.size()` which contains the drop target
  // if it exists, and if two windows are in a snap group, they are a single
  // item.
  size_t GetNumWindows() const;

  // Returns the save desk as template button if available, otherwise null.
  SavedDeskSaveDeskButton* GetSaveDeskAsTemplateButton();

  // Returns the save desk for later button if available, otherwise null.
  SavedDeskSaveDeskButton* GetSaveDeskForLaterButton();

  // Returns the save button container if available, otherwise null.
  SavedDeskSaveDeskButtonContainer* GetSaveDeskButtonContainer();
  const SavedDeskSaveDeskButtonContainer* GetSaveDeskButtonContainer() const;

  const SplitViewSetupView* GetSplitViewSetupView() const;

  // Gets the cropping area of the wallpaper in screen coordinates.
  gfx::Rect GetWallpaperClipBounds() const;

  // Initializes the widget that contains the `BirchBarView` contents.`by_user`
  // is true, if the user selects to show the birch bar from the context menu.
  void MaybeInitBirchBarWidget(bool by_user = false);

  // Shuts down birch bar widget, when the user selects to hide the birch bar
  // from the context menu.
  void ShutdownBirchBarWidgetByUser();

  // Destroys the birch bar widget, clears pointers and refresh grids. `by_user`
  // is true when the birch bar is disabled by user.
  void DestroyBirchBarWidget(bool by_user = false);

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

  // OverviewItem::WindowDestructionDelegate:
  void OnOverviewItemWindowDestroying(OverviewItem* overview_item,
                                      bool reposition) override;

  // Returns the saved desk library view, or nullptr.
  SavedDeskLibraryView* GetSavedDeskLibraryView();
  const SavedDeskLibraryView* GetSavedDeskLibraryView() const;

  // Returns true if the grid has no more items.
  bool empty() const { return item_list_.empty(); }

  const OverviewDropTarget* drop_target() const { return drop_target_; }

  // Returns the root window in which the grid displays the windows.
  aura::Window* root_window() { return root_window_; }
  const aura::Window* root_window() const { return root_window_; }

  OverviewSession* overview_session() { return overview_session_; }

  const std::vector<std::unique_ptr<OverviewItemBase>>& item_list() const {
    return item_list_;
  }

  const SplitViewDragIndicators* split_view_drag_indicators() const {
    return split_view_drag_indicators_.get();
  }

  bool should_animate_when_exiting() const {
    return should_animate_when_exiting_;
  }

  void set_suspend_reposition(bool value) { suspend_reposition_ = value; }

  OverviewGridEventHandler* grid_event_handler() {
    return grid_event_handler_.get();
  }

  aura::Window* dragged_window() { return dragged_window_.get(); }

  // TODO(sammiequon): Remove some of these getters by using friend or helper
  // function.
  RoundedLabelWidget* no_windows_widget() { return no_windows_widget_.get(); }

  const views::Widget* desks_widget() const { return desks_widget_.get(); }
  views::Widget* desks_widget() { return desks_widget_.get(); }

  const OverviewDeskBarView* desks_bar_view() const { return desks_bar_view_; }
  OverviewDeskBarView* desks_bar_view() { return desks_bar_view_; }

  views::Widget* birch_bar_widget() { return birch_bar_widget_.get(); }

  views::Widget* split_view_setup_widget() {
    return split_view_setup_widget_.get();
  }

  views::Widget* saved_desk_library_widget() {
    return saved_desk_library_widget_.get();
  }

  views::Widget* informed_restore_widget() {
    return informed_restore_widget_.get();
  }
  const views::Widget* informed_restore_widget() const {
    return informed_restore_widget_.get();
  }

  views::Widget* save_desk_button_container_widget() {
    return save_desk_button_container_widget_.get();
  }

  ScopedOverviewWallpaperClipper* scoped_overview_wallpaper_clipper() {
    return scoped_overview_wallpaper_clipper_.get();
  }

  int num_incognito_windows() const { return num_incognito_windows_; }

  int num_unsupported_windows() const { return num_unsupported_windows_; }

  OverviewUiTaskPool& enter_animation_task_pool() {
    return enter_animation_task_pool_;
  }

  SaveDeskOptionStatus GetEnableStateAndTooltipIDForTemplateType(
      DeskTemplateType type) const;

  base::WeakPtr<OverviewGrid> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  friend class DesksTemplatesTest;
  friend class OverviewGridTestApi;
  friend class OverviewTestBase;

  // Struct which holds data required to perform nudges. Nudge in the context of
  // overview view means an overview item is currently being dragged vertically
  // and may be closed when released, and the remaining windows will move
  // towards their positions once the item is closed.
  struct OverviewNudgeData {
    size_t index;
    gfx::RectF src;
    gfx::RectF dst;
  };

  // Initializes the widget that contains the `OverviewDeskBarView` contents.
  // Also will update the save desk buttons visibility after we initialize
  // `OverviewDeskBarView`.
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
      const base::flat_set<OverviewItemBase*>& ignored_items);

  // Gets the layout of the overview items. Currently only for tablet mode.
  // Positions up to six windows into two rows of equal height, scaling each
  // window to fit that height. Additional windows are placed off-screen.
  // `ignored_items` won't be shown along with the other windows in overview
  // mode.
  std::vector<gfx::RectF> GetWindowRectsForScrollingLayout(
      const base::flat_set<OverviewItemBase*>& ignored_items);

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
      const base::flat_set<OverviewItemBase*>& ignored_items,
      std::vector<gfx::RectF>* out_rects,
      int* out_max_bottom,
      int* out_min_right,
      int* out_max_right);

  // Maybe modify `out_window_rects` to center the overview items excluding the
  // the rect(s) corresponding to item(s) in `ignored_items`.
  void MaybeCenterOverviewItems(
      const base::flat_set<OverviewItemBase*>& ignored_items,
      std::vector<gfx::RectF>& out_window_rects);

  // Returns the index of `item` in `item_list_`.
  size_t GetOverviewItemIndex(OverviewItemBase* item) const;

  // Returns the index where `window` can be inserted into `item_list_` based
  // on MRU order.
  size_t FindInsertionIndex(const aura::Window* window) const;

  // Adds the |dragged_window| into overview on drag ended. Might need to update
  // the window's bounds if it has been resized.
  void AddDraggedWindowIntoOverviewOnDragEnd(aura::Window* dragged_window);

  // Returns the the bounds of the desks widget in screen coordinates.
  gfx::Rect GetDesksWidgetBounds() const;

  // Returns the bounds of the birch bar widget in the screen coordinates.
  gfx::Rect GetBirchBarWidgetBounds() const;

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

  // Called when the layout of the birch bar is updated. We may need to
  // reposition the windows if the relayout is due to the contents change.
  void OnBirchBarLayoutChanged(BirchBarView::RelayoutReason reason);

  // Refreshes desks widgets visibility: hidden in partial Overview, visibility
  // restored when partial Overview ends.
  void RefreshDesksWidgets(bool visible);

  // Updates the number of unsupported windows of saved desk. This includes
  // `num_incognito_windows_` and `num_unsupported_windows` as of now. When
  // the overview item that represents the `windows` is being added to `this`,
  // `increment` is true, and false if being removed.
  void UpdateNumSavedDeskUnsupportedWindows(
      const std::vector<raw_ptr<aura::Window, VectorExperimental>>& windows,
      bool increment);

  // Returns the height of `desks_bar_view_`.
  int GetDesksBarHeight() const;

  bool ShouldUseScrollingLayout(size_t ignored_items_size) const;

  // Creates the drop target, which lets users know where `dragged_item` will
  // land. Adds the drop target to `item_list_` at `position` (which is
  // usually the index of `dragged_item`), and calls `PositionWindows()`.
  void AddDropTargetImpl(OverviewItemBase* dragged_item,
                         size_t position,
                         bool animate);

  // Called when the split view setup view toast skip button is pressed.
  void OnSkipButtonPressed();

  // Called when the split view setup view settings button is pressed.
  void OnSettingsButtonPressed();

  // Updates the visibility of `split_view_setup_widget_`. The widget will
  // only be shown if split view overview is in session.
  void UpdateSplitViewSetupViewWidget();

  // Whether the `desks_widget_` should be initialized.
  bool ShouldInitDesksWidget() const;

  // The drop target is created when a window or overview item is being dragged,
  // and is destroyed when the drag ends or overview mode is ended. The drop
  // target is hidden when a snap preview area is shown. You can drop a window
  // into overview by dragging to the drop target or by dragging to almost
  // anywhere while the drop target is shown. The drop target is owned by
  // `item_list_`; this is just a convenience pointer.
  raw_ptr<OverviewDropTarget> drop_target_ = nullptr;

  // Root window the grid is in.
  raw_ptr<aura::Window, DanglingUntriaged> root_window_;

  // Pointer to the OverviewSession that spawned this grid.
  raw_ptr<OverviewSession> overview_session_;

  // Vector containing all the items in this grid.
  std::vector<std::unique_ptr<OverviewItemBase>> item_list_;

  // The owner of the widget that displays split-view-related information. Null
  // if split view is unsupported (see |ShouldAllowSplitView|).
  std::unique_ptr<SplitViewDragIndicators> split_view_drag_indicators_;

  // A widget that is shown if we entered overview without any windows opened.
  std::unique_ptr<RoundedLabelWidget> no_windows_widget_;

  // Widget that contains the DeskBarView contents when the Virtual Desks
  // feature is enabled.
  std::unique_ptr<views::Widget> desks_widget_;

  // The contents view of the above |desks_widget_| if created.
  raw_ptr<OverviewDeskBarView, DanglingUntriaged> desks_bar_view_ = nullptr;

  // Widget that contains the BirchBarView contents when the Forest feature is
  // enabled.
  std::unique_ptr<views::Widget> birch_bar_widget_;

  // The contents view of the `birch_bar_widget_` if created.
  raw_ptr<BirchBarView> birch_bar_view_ = nullptr;

  // Widget that appears during the split view setup. Contains the split view
  // setup view toast and settings button.
  std::unique_ptr<views::Widget> split_view_setup_widget_;

  // The widget that contains the view for all saved desks.
  std::unique_ptr<views::Widget> saved_desk_library_widget_;

  // The widget that contains the `InformedRestoreContentsView`.
  std::unique_ptr<views::Widget> informed_restore_widget_;

  // A widget that contains save desk buttons which save desk as template or for
  // later when pressed.
  std::unique_ptr<views::Widget> save_desk_button_container_widget_;

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
  std::vector<OverviewNudgeData> nudge_data_;

  // Measures the animation smoothness of overview animation.
  std::unique_ptr<MetricsTracker> metrics_tracker_;

  // True to skip |PositionWindows()|. Used to avoid O(n^2) layout when
  // reposition windows in tablet overview mode.
  bool suspend_reposition_ = false;

  // Used by `GetWindowRectsForScrollingLayout()` to shift the x position of the
  // overview items.
  float scroll_offset_ = 0;

  // Value to clamp `scroll_offset_` so scrolling stays limited to windows that
  // are visible in tablet overview mode.
  float scroll_offset_min_ = 0.f;

  // Handles events that are not handled by the OverviewItems.
  std::unique_ptr<OverviewGridEventHandler> grid_event_handler_;

  // Hides scoped windows in partial overview, restores their visibility when
  // partial overview ends.
  std::unique_ptr<ScopedOverviewHideWindows> hide_windows_in_partial_overview_;

  // Records the presentation time of scrolling the grid in overview mode.
  std::unique_ptr<ui::PresentationTimeRecorder> presentation_time_recorder_;

  // Window that is being dragged from the shelf or during tab dragging.
  raw_ptr<aura::Window> dragged_window_ = nullptr;

  //  A scoped object responsible for managing wallpaper clipping transitions
  //  during overview mode.
  std::unique_ptr<ScopedOverviewWallpaperClipper>
      scoped_overview_wallpaper_clipper_;

  // The number of incognito windows in this grid. Used by saved desks to
  // identify the unsupported window type to the user.
  int num_incognito_windows_ = 0;

  // The number of unsupported windows in this grid. Used by saved desks to
  // identify the unsupported window type to the user.
  int num_unsupported_windows_ = 0;

  // Used when feature ContinuousOverviewScrollAnimation is enabled. When a
  // continuous scroll starts, store the calculated target transforms here. For
  // each scroll update, use this list to prevent unnecessary recalculations.
  // For a scroll end, clear the list.
  base::flat_map<OverviewItemBase*, gfx::Transform> cached_transforms_;

  std::optional<OverviewController::ScopedOcclusionPauser> rotation_pauser_;
  std::optional<OverviewController::ScopedOcclusionPauser> scroll_pauser_;

  const base::WeakPtr<WindowOcclusionCalculator> window_occlusion_calculator_;

  // Set of tasks that get run on the UI thread while the enter-animation is
  // in progress. These tasks are not immediately necessary but may be after the
  // enter-animation is complete.
  OverviewUiTaskPool enter_animation_task_pool_;

  base::WeakPtrFactory<OverviewGrid> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_OVERVIEW_GRID_H_
