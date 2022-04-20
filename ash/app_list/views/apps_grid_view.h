// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_APPS_GRID_VIEW_H_
#define ASH_APP_LIST_VIEWS_APPS_GRID_VIEW_H_

#include <stddef.h>

#include <memory>
#include <queue>
#include <set>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

#include "ash/app_list/app_list_metrics.h"
#include "ash/app_list/grid_index.h"
#include "ash/app_list/model/app_list_item_list_observer.h"
#include "ash/app_list/model/app_list_model_observer.h"
#include "ash/app_list/paged_view_structure.h"
#include "ash/app_list/views/app_list_item_view.h"
#include "ash/ash_export.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/models/list_model_observer.h"
#include "ui/compositor/throughput_tracker.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/animation/animation_abort_handle.h"
#include "ui/views/animation/bounds_animator_observer.h"
#include "ui/views/view.h"
#include "ui/views/view_model.h"

namespace views {
class BoundsAnimator;
}  // namespace views

namespace ash {

namespace test {
class AppsGridViewTest;
class AppsGridViewTestApi;
}  // namespace test

class AppDragIconProxy;
class AppListA11yAnnouncer;
class ApplicationDragAndDropHost;
class AppListConfig;
class AppListFolderController;
class AppListItem;
class AppListItemList;
class AppListItemView;
class AppListModel;
class AppListViewDelegate;
class AppsGridContextMenu;
class AppsGridViewFocusDelegate;
class AppsGridViewFolderDelegate;
class PulsingBlockView;
class GhostImageView;
class AppsGridViewTest;
class ScrollableAppsGridViewTest;

// AppsGridView displays a grid of app icons. It is used for:
// - The main grid of apps in the launcher
// - The grid of apps in a folder
class ASH_EXPORT AppsGridView : public views::View,
                                public AppListItemView::GridDelegate,
                                public AppListItemListObserver,
                                public AppListModelObserver,
                                public views::BoundsAnimatorObserver {
 public:
  METADATA_HEADER(AppsGridView);

  enum Pointer {
    NONE,
    MOUSE,
    TOUCH,
  };

  AppsGridView(AppListA11yAnnouncer* a11y_announcer,
               AppListViewDelegate* app_list_view_delegate,
               AppsGridViewFolderDelegate* folder_delegate,
               AppListFolderController* folder_controller,
               AppsGridViewFocusDelegate* focus_delegate);
  AppsGridView(const AppsGridView&) = delete;
  AppsGridView& operator=(const AppsGridView&) = delete;
  ~AppsGridView() override;

  // Initializes the class. Calls virtual methods, so its code cannot be in the
  // constructor.
  void Init();

  // Sets the `AppListConfig` that should be used to configure app list item
  // size within the grid. This will cause all items views to be updated to
  // adhere to new tile and icon dimensions, so it should be used sparingly.
  void UpdateAppListConfig(const AppListConfig* app_list_config);

  int cols() const { return cols_; }

  // Sets padding for apps grid items to use during layout if fixed padding
  // should be used. Otherwise, for paged apps grid, the padding will be
  // calculated to evenly space the items within the current apps grid view
  // bounds.
  void SetFixedTilePadding(int horizontal_tile_padding,
                           int vertical_tile_padding);

  // Returns the size of a tile view including its padding. For paged apps grid,
  // padding can be different between tiles on the first page and tiles on other
  // pages.
  gfx::Size GetTotalTileSize(int page) const;

  // Returns the minimum size of the entire tile grid.
  gfx::Size GetMinimumTileGridSize(int cols, int rows_per_page) const;

  // Returns the maximum size of the entire tile grid.
  gfx::Size GetMaximumTileGridSize(int cols, int rows_per_page) const;

  // Cancels any in progress drag without running icon drop animation. If an
  // icon drop animation is in progress, it will be canceled, too.
  void CancelDragWithNoDropAnimation();

  // This resets the grid view to a fresh state for showing the app list.
  void ResetForShowApps();

  // All items in this view become unfocusable if |disabled| is true. This is
  // used to trap focus within the folder when it is opened.
  void DisableFocusForShowingActiveFolder(bool disabled);

  // Sets |model| to use. Note this does not take ownership of |model|.
  void SetModel(AppListModel* model);

  // Sets the |item_list| to render. Note this does not take ownership of
  // |item_list|.
  void SetItemList(AppListItemList* item_list);

  // AppListItemView::GridDelegate:
  bool IsInFolder() const override;
  void SetSelectedView(AppListItemView* view) override;
  void ClearSelectedView() override;
  bool IsSelectedView(const AppListItemView* view) const override;
  bool InitiateDrag(AppListItemView* view,
                    const gfx::Point& location,
                    const gfx::Point& root_location,
                    base::OnceClosure drag_start_callback,
                    base::OnceClosure drag_end_callback) override;
  void StartDragAndDropHostDragAfterLongPress() override;
  bool UpdateDragFromItem(bool is_touch,
                          const ui::LocatedEvent& event) override;
  void EndDrag(bool cancel) override;
  void OnAppListItemViewActivated(AppListItemView* pressed_item_view,
                                  const ui::Event& event) override;

  bool IsDragging() const;
  bool IsDraggedView(const AppListItemView* view) const;

  void ClearDragState();

  // Set the drag and drop host for application links.
  void SetDragAndDropHostOfCurrentAppList(
      ApplicationDragAndDropHost* drag_and_drop_host);

  // Return true if the |bounds_animator_| is animating |view|.
  bool IsAnimatingView(AppListItemView* view);

  const AppListConfig* app_list_config() const { return app_list_config_; }

  bool has_selected_view() const { return selected_view_ != nullptr; }
  AppListItemView* selected_view() const { return selected_view_; }

  const AppListItemView* drag_view() const { return drag_view_; }

  bool has_dragged_item() const { return drag_item_ != nullptr; }
  const AppListItem* drag_item() const { return drag_item_; }

  // Overridden from views::View:
  gfx::Size CalculatePreferredSize() const override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  bool OnKeyReleased(const ui::KeyEvent& event) override;
  void ViewHierarchyChanged(
      const views::ViewHierarchyChangedDetails& details) override;
  bool GetDropFormats(int* formats,
                      std::set<ui::ClipboardFormatType>* format_types) override;
  bool CanDrop(const OSExchangeData& data) override;
  int OnDragUpdated(const ui::DropTargetEvent& event) override;

  // Updates the visibility of app list items according to |app_list_state| and
  // |is_in_drag|.
  void UpdateControlVisibility(AppListViewState app_list_state,
                               bool is_in_drag);

  // Returns true if a touch or click lies between two occupied tiles.
  bool EventIsBetweenOccupiedTiles(const ui::LocatedEvent* event);

  // Returns the item view of the item with the provided item ID.
  // Returns nullptr if there is no such item.
  AppListItemView* GetItemViewForItem(const std::string& item_id);

  // Returns the item view of the item at |index|, or nullptr if there is no
  // view at |index|.
  AppListItemView* GetItemViewAt(int index) const;

  // Called to initiate drag for reparenting a folder item in root level grid
  // view.
  // `pointer` - The pointer that's used for dragging (mouse or touch).
  // `drag_point` is in the coordinates of root level grid view.
  // `cancellation_callback` - the callback that can be invoked from the root
  // level grid to cancel drag operation in the originating folder grid.
  void InitiateDragFromReparentItemInRootLevelGridView(
      Pointer pointer,
      AppListItemView* original_drag_view,
      const gfx::Point& drag_point,
      base::OnceClosure cancellation_callback);

  // Updates drag in the root level grid view when receiving the drag event
  // dispatched from the hidden grid view for reparenting a folder item.
  // `pointer` - The pointer that's used for dragging (mouse or touch).
  void UpdateDragFromReparentItem(Pointer pointer,
                                  const gfx::Point& drag_point);

  // Dispatches the drag event from hidden grid view to the top level grid view.
  // `pointer` - The pointer that's used for dragging (mouse or touch).
  void DispatchDragEventForReparent(Pointer pointer,
                                    const gfx::Point& drag_point);

  // Handles EndDrag event dispatched from the hidden folder grid view in the
  // root level grid view to end reparenting a folder item.
  // |original_parent_item_view|: The folder AppListView for the folder from
  // which drag item is being dragged.
  // |events_forwarded_to_drag_drop_host|: True if the dragged item is dropped
  // to the drag_drop_host, eg. dropped on shelf.
  // |cancel_drag|: True if the drag is ending because it has been canceled.
  // |drag_icon_proxy|: The app item drag icon proxy that was created by the
  // folder grid view for the drag. It's passed on the the root apps grid so the
  // root apps grid can set up the icon drop animation.
  void EndDragFromReparentItemInRootLevel(
      AppListItemView* original_parent_item_view,
      bool events_forwarded_to_drag_drop_host,
      bool cancel_drag,
      std::unique_ptr<AppDragIconProxy> drag_icon_proxy);

  // Handles EndDrag event in the hidden folder grid view to end reparenting
  // a folder item.
  void EndDragForReparentInHiddenFolderGridView();

  // Moves |reparented_view| from its folder to the root AppsGridView in the
  // direction of |key_code|.
  // |original_parent_item_view|: The folder AppListView for the folder from
  // which drag item is being dragged.
  void HandleKeyboardReparent(AppListItemView* reparented_view,
                              AppListItemView* original_parent_item_view,
                              ui::KeyboardCode key_code);

  // Updates paged view structure and save it to meta data.
  void UpdatePagedViewStructure();

  // Returns true if tablet mode is active. This class does not use
  // Shell::IsInTabletMode() because it has tests that are not derived from
  // AshTestBase.
  bool IsTabletMode() const;

  // Fades out visible items when reordering happens. Runs `done_callback` when
  // the fade out animation ends. The callback carries a boolean value that
  // is true if the animation is aborted. Returns an animation builder and it
  // can be used to tie other layer animations with scheduled item animaions.
  using ReorderAnimationCallback = base::RepeatingCallback<void(bool)>;
  views::AnimationBuilder FadeOutVisibleItemsForReorder(
      ReorderAnimationCallback done_callback);

  // Fades in items for reordering. Returns an animation builder and it can be
  // used to tie other layer animations with scheduled item animaions.
  views::AnimationBuilder FadeInVisibleItemsForReorder(
      ReorderAnimationCallback done_callback);

  // Whether the provided view is hidden to facilitate drag operation (for
  // example, the drag view for which a drag icon proxy has been created).
  bool IsViewHiddenForDrag(const views::View* view) const;

  // Whether `view` is the folder view that is animating out and in as part of
  // folder reorder animation that runs after folder is closed if the folder
  // position within the grid changed.
  bool IsViewHiddenForFolderReorder(const views::View* view) const;

  // Returns true if the apps grid is under the reorder animation process. This
  // function is public for testing.
  bool IsUnderReorderAnimation() const;

  // Aborts the active reorder animation if any.
  void MaybeAbortReorderAnimation();

  // Passes scroll information from a parent view, so that subclasses may scroll
  // or switch pages.
  virtual void HandleScrollFromParentView(const gfx::Vector2d& offset,
                                          ui::EventType type) = 0;

  // Return the view model.
  views::ViewModelT<AppListItemView>* view_model() { return &view_model_; }
  const views::ViewModelT<AppListItemView>* view_model() const {
    return &view_model_;
  }

  // Returns true if any animation is running within the view.
  bool IsAnimationRunningForTest();
  // Cancel any animations currently running within the view.
  void CancelAnimationsForTest();

  AppsGridViewFolderDelegate* folder_delegate() const {
    return folder_delegate_;
  }

  void set_folder_delegate(AppsGridViewFolderDelegate* folder_delegate) {
    folder_delegate_ = folder_delegate;
  }

  const AppListModel* model() const { return model_; }

  GridIndex reorder_placeholder() const { return reorder_placeholder_; }

  bool FireFolderItemReparentTimerForTest();
  bool FireDragToShelfTimerForTest();

  // Carries two parameters:
  // (1) A boolean value that is true if the reorder is aborted.
  // (2) An enum that specifies the animation stage when the done callback runs.
  using TestReorderDoneCallbackType =
      base::RepeatingCallback<void(bool aborted,
                                   AppListReorderAnimationStatus status)>;

  // Adds a callback that runs at the end of the app list reorder.
  void AddReorderCallbackForTest(TestReorderDoneCallbackType done_callback);

  // Adds a closure that runs at the start of the fade out animation triggered
  // by reorder.
  void AddFadeOutAnimationStartClosureForTest(base::OnceClosure start_closure);

  // Adds a closure that runs at the end of the fade out animation triggered
  // by reorder.
  void AddFadeOutAnimationDoneClosureForTest(base::OnceClosure done_closure);

  // Returns true if there is any waiting reorder animation test callback.
  bool HasAnyWaitingReorderDoneCallbackForTest() const;

  // For test: Return if the drag and drop handler was set.
  bool has_drag_and_drop_host_for_test() {
    return nullptr != drag_and_drop_host_;
  }

  // For test: Return if the drag and drop operation gets dispatched.
  bool forward_events_to_drag_and_drop_host_for_test() {
    return forward_events_to_drag_and_drop_host_;
  }

  views::BoundsAnimator* bounds_animator_for_testing() {
    return bounds_animator_.get();
  }

  base::OneShotTimer* reorder_timer_for_test() { return &reorder_timer_; }

  AppsGridContextMenu* context_menu_for_test() { return context_menu_.get(); }

  void set_enable_item_move_animation_for_test(bool enable) {
    enable_item_move_animation_ = enable;
  }

  AppListReorderAnimationStatus reorder_animation_status_for_test() const {
    return reorder_animation_status_;
  }

 protected:
  friend AppsGridViewTest;
  friend ScrollableAppsGridViewTest;

  struct VisibleItemIndexRange {
    VisibleItemIndexRange();
    VisibleItemIndexRange(int first_index, int last_index);
    ~VisibleItemIndexRange();

    // The view index of the first visible item on the apps grid.
    int first_index = 0;

    // The view index of the last visible item on the apps grid.
    int last_index = 0;
  };

  // The duration in ms for most of the apps grid view animations.
  static constexpr int kDefaultAnimationDuration = 200;

  // Returns the size of a tile view excluding its padding.
  virtual gfx::Size GetTileViewSize() const = 0;

  // Returns the padding around a tile view.
  virtual gfx::Insets GetTilePadding(int page) const = 0;

  // Returns the size of the entire tile grid.
  virtual gfx::Size GetTileGridSize() const = 0;

  // Returns the max number of rows the grid can have on a page.
  virtual int GetMaxRowsInPage(int page) const = 0;

  // Calculates the offset distance to center the grid in the container.
  virtual gfx::Vector2d GetGridCenteringOffset(int page) const = 0;

  // Returns number of total pages, or one if the grid does not use pages.
  virtual int GetTotalPages() const = 0;

  // Returns the current selected page, or zero if the grid does not use pages.
  virtual int GetSelectedPage() const = 0;

  // Returns true if scrolling is vertical (the common case). Folders may scroll
  // horizontally.
  virtual bool IsScrollAxisVertical() const = 0;

  // Records the different ways to move an app in app list's apps grid for UMA
  // histograms.
  virtual void RecordAppMovingTypeMetrics(AppListAppMovingType type) = 0;

  // Updates or creates a border for this view.
  virtual void UpdateBorder() {}

  // Starts the "cardified" state if the subclass supports it.
  virtual void MaybeStartCardifiedView() {}

  // Ends the "cardified" state if the subclass supports it.
  virtual void MaybeEndCardifiedView() {}

  // Starts a page flip if the subclass supports it.
  virtual void MaybeStartPageFlip() {}

  // Stops a page flip (by ending its timer) if the subclass supports it.
  virtual void MaybeStopPageFlip() {}

  // Scrolls the container view up or down if the drag point is in the correct
  // location. Returns true if auto-scroll was started or an existing
  // auto-scroll is in-progress. Auto-scroll and page-flip are mutually
  // exclusive. TODO(tbarzic): Unify the two APIs.
  virtual bool MaybeAutoScroll() = 0;

  // Stops auto-scroll (by stopping its timer) if the subclass supports it.
  virtual void StopAutoScroll() = 0;

  // Sets the focus to the correct view when a drag ends. Focus is on the app
  // list item view during the drag.
  virtual void SetFocusAfterEndDrag() = 0;

  // Calculates the index range of the visible item views.
  virtual absl::optional<VisibleItemIndexRange> GetVisibleItemIndexRange()
      const = 0;

  // Disables any change on the apps grid's opacity. Returns an scoped runner
  // that carries a closure to re-enable opacity updates.
  [[nodiscard]] virtual base::ScopedClosureRunner LockAppsGridOpacity() = 0;

  // Makes sure that the background cards render behind everything
  // else in the items container.
  virtual void StackCardsAtBottom() {}

  // Sets the max number of columns that the grid can have.
  // For root apps grid view, the grid size depends on the space available to
  // apps grid view only, and `cols()` will match `max_columns`. I.e. if the
  // grid doesn't have enough items to fill out all columns, it will leave empty
  // spaces in the UI.
  // For folder item grid, the grid size also depends on the number of items in
  // the grid, so number of actual columns may be smaller than `max_columns`.
  void SetMaxColumnsInternal(int max_columns);

  // Sets the ideal bounds for view at index `view_inde_in_model` in
  // `view_model_`. The bounds are set to match the expected tile bounds at
  // `view_grid_index` in the apps grid.
  void SetIdealBoundsForViewToGridIndex(int view_index_in_model,
                                        const GridIndex& view_grid_index);

  // Calculates the item views' bounds for both folder and non-folder.
  void CalculateIdealBounds();
  void CalculateIdealBoundsForPageStructureWithPartialPages();

  // Gets the bounds of the tile located at |index|, where |index| contains the
  // page/slot info.
  gfx::Rect GetExpectedTileBounds(const GridIndex& index) const;

  // Returns the number of app tiles per page. Takes a page number as an
  // argument as the first page might have less apps shown. Folder grids may
  // have different numbers of tiles from the main grid.
  int TilesPerPage(int page) const;

  GridIndex GetIndexOfView(const AppListItemView* view) const;
  AppListItemView* GetViewAtIndex(const GridIndex& index) const;

  // Returns true if an item view exists in the visual index.
  bool IsValidIndex(const GridIndex& index) const;

  // Returns the number of existing items in specified page. Returns 0 if |page|
  // is out of range.
  int GetNumberOfItemsOnPage(int page) const;

  // Updates |drop_target_| and |drop_target_region_| based on |drag_view_|'s
  // position.
  void UpdateDropTargetRegion();

  // Cancels any context menus showing for app items on the current page.
  void CancelContextMenusOnCurrentPage();

  // views::BoundsAnimatorObserver:
  void OnBoundsAnimatorProgressed(views::BoundsAnimator* animator) override;
  void OnBoundsAnimatorDone(views::BoundsAnimator* animator) override;

  // Whether app list item views require layers - for example during drag, or
  // folder repositioning animation.
  bool ItemViewsRequireLayers() const;

  void BeginHideCurrentGhostImageView();

  // Ensures layer for all app items before animations are started.
  void PrepareItemsForBoundsAnimation();

  bool ignore_layout() const { return ignore_layout_; }
  views::BoundsAnimator* bounds_animator() { return bounds_animator_.get(); }
  views::View* items_container() { return items_container_; }
  views::ViewModelT<PulsingBlockView>& pulsing_blocks_model() {
    return pulsing_blocks_model_;
  }
  const gfx::Point& last_drag_point() const { return last_drag_point_; }
  void set_last_drag_point(const gfx::Point& p) { last_drag_point_ = p; }
  bool handling_keyboard_move() const { return handling_keyboard_move_; }

  AppListViewDelegate* app_list_view_delegate() const {
    return app_list_view_delegate_;
  }
  const AppListItemList* item_list() const { return item_list_; }

  // View structure used only for non-folder.
  PagedViewStructure view_structure_{this};

  // The `AppListItemView` that is being dragged within the apps grid (i.e. the
  // AppListItemView for `drag_item_`) if the drag item is currently part of the
  // item list shown in the apps grid. `drag_view_` may be nullptr during item
  // reparent drag while being handled in the root app list grid (the drag item
  // will be added to target item list only when the drag ends).
  // Subclasses need non-const access.
  AppListItemView* drag_view_ = nullptr;

  // If set, a callback called when the dragged item starts moving during a drag
  // (i.e. when the drag icon proxy gets created).
  // Registered in `InitiateDrag()`
  base::OnceClosure drag_start_callback_;

  // If set, a callback called when an item drag ends, and drag state is
  // cleared. It may get called before the drag icon proxy drop animation
  // finishes.
  // Registered in `InitiateDrag()`.
  base::OnceClosure drag_end_callback_;

  // If app item drag is in progress, the icon proxy created for the app list
  // item.
  std::unique_ptr<AppDragIconProxy> drag_icon_proxy_;

  // If true, Layout() does nothing. See where set for details.
  bool ignore_layout_ = false;

  // True if the AppList is in cardified state. "Cardified" means showing a
  // rounded rectangle background "card" behind each page during a drag. The
  // grid icons are reduced in size in this state.
  // TODO(crbug.com/1211608): Move cardified state members to PagedAppsGridView.
  bool cardified_state_ = false;

  int bounds_animation_for_cardified_state_in_progress_ = 0;

  // Tile spacing between the tile views.
  int horizontal_tile_padding_ = 0;
  int vertical_tile_padding_ = 0;

  // Whether tile padding within the apps grid is fixed.
  bool has_fixed_tile_padding_ = false;

  // True if an extra page is opened after the user drags an app to the bottom
  // of last page with intention to put it in a new page. This is only used for
  // non-folder.
  bool extra_page_opened_ = false;

  GhostImageView* current_ghost_view_ = nullptr;
  GhostImageView* last_ghost_view_ = nullptr;

 private:
  friend class test::AppsGridViewTestApi;
  friend class test::AppsGridViewTest;
  friend class PagedAppsGridView;
  friend class PagedViewStructure;

  enum DropTargetRegion {
    NO_TARGET,
    ON_ITEM,
    NEAR_ITEM,
    BETWEEN_ITEMS,
  };

  class DragViewHider;
  class FolderIconItemHider;

  // Updates from model.
  void Update();

  // Updates page splits for item views.
  virtual void UpdatePaging() {}

  // On a grid with pages, records the total number of pages, and the number of
  // pages with empty slots for UMA histograms.
  virtual void RecordPageMetrics() {}

  // Calculates the offset for |page_of_view| based on current page and
  // transition target page. Returns an empty vector if the grid does not use
  // pages.
  virtual const gfx::Vector2d CalculateTransitionOffset(
      int page_of_view) const = 0;

  // Calculates the animation delay for the pulsing block animation based on the
  // position of the block.
  base::TimeDelta GetPulsingBlockAnimationDelayForIndex(int block_index);

  // Invoked when the animation for swapping a |placeholder| with an |app_view|
  // is done.
  void OnSwapAnimationDone(views::View* placeholder, AppListItemView* app_view);

  // Triggers the animation for swapping a pulsing block view with a
  // corresponding new asset at index on item list. (We use the item_list index
  // because the view is not added to the `view_model_` yet.)
  // Only triggers when there are pulsing blocks and the app list model is
  // syncing.
  AppListItemView* MaybeSwapPlaceholderAsset(size_t index);

  // Updates the number of pulsing block views based on AppListModel status and
  // number of apps.
  void UpdatePulsingBlockViews();

  std::unique_ptr<AppListItemView> CreateViewForItemAtIndex(size_t index);

  // Ensures the view is visible. Note that if there is a running page
  // transition, this does nothing.
  virtual void EnsureViewVisible(const GridIndex& index) = 0;

  void SetSelectedItemByIndex(const GridIndex& index);

  // Calculates ideal bounds for app list item views within the apps grid, and
  // animates their bounds (using `bounds_animator_`) to their ideal position.
  void AnimateToIdealBounds();

  // Invoked when the given |view|'s current bounds and target bounds are on
  // different rows. To avoid moving diagonally, |view| would be put into a
  // slot prior |target| and fade in while moving to |target|. In the meanwhile,
  // a layer copy of |view| would start at |current| and fade out while moving
  // to succeeding slot of |current|. |animate_current| controls whether to run
  // fading out animation from |current|. |animate_target| controls whether to
  // run fading in animation to |target|.
  void AnimationBetweenRows(AppListItemView* view,
                            bool animate_current,
                            const gfx::Rect& current,
                            bool animate_target,
                            const gfx::Rect& target);

  // Extracts drag location info from |root_location| into |drag_point|.
  void ExtractDragLocation(const gfx::Point& root_location,
                           gfx::Point* drag_point);

  bool DropTargetIsValidFolder();

  // Updates |drop_target_| as a location for potential reordering after the
  // currently dragged item is released.
  void UpdateDropTargetForReorder(const gfx::Point& point);

  // Called when the user is dragging an app. |point| is in grid view
  // coordinates.
  // `pointer` - The pointer that's used for dragging (mouse or touch).
  void UpdateDrag(Pointer pointer, const gfx::Point& point);

  // Returns true if the current drag is occurring within a certain range of the
  // nearest item.
  // `point` is the drag location in the apps grid's coordinates.
  bool DragIsCloseToItem(const gfx::Point& point);

  // Whether the current drag position is over an item.
  // `point` is the drag location in the apps grid's coordinates.
  bool DragPointIsOverItem(const gfx::Point& point);

  // Initiates drag and drop host drag as needed.
  // `pointer` - The pointer that's used for dragging.
  void TryStartDragAndDropHostDrag(Pointer pointer);

  // Prepares |drag_and_drop_host_| for dragging. |grid_location| contains
  // the drag point in this grid view's coordinates.
  void StartDragAndDropHostDrag();

  // Dispatch the drag and drop update event to the dnd host (if needed).
  void DispatchDragEventToDragAndDropHost(
      const gfx::Point& location_in_screen_coordinates);

  // Returns whether the target grid index for item move operation is on a new
  // apps grid page - i.e. if the move operation will create a new apps grid
  // page. Used to determine whether a new page break should be created after
  // app list item move.
  bool IsMoveTargetOnNewPage(const GridIndex& target) const;

  // Creates a page break just before the item in top level item list if the
  // item is not already preceded by a page break.
  void EnsurePageBreakBeforeItem(const std::string& item_id);

  // Updates `model_` to move `item` to `target` slot.
  void MoveItemInModel(AppListItem* item, const GridIndex& target);

  // Updates `model_` to move `item` into a folder containing item located at
  // `target` slot. Returns whether the move operation succeeded.
  // On success, `folder_id` will be set to the ID of the folder to which the
  // item was moved. This may be a folder that was created by the move, or a
  // preexisting folder.
  // `is_new_folder` indicates whether the move created a new folder.
  bool MoveItemToFolder(AppListItem* item,
                        const GridIndex& target,
                        AppListAppMovingType move_type,
                        std::string* folder_id,
                        bool* is_new_folder);

  // Updates data model for re-parenting a folder item to a new position in top
  // level item list. The view model is will get updated in response to the data
  // model changes.
  void ReparentItemForReorder(AppListItem* item, const GridIndex& target);

  // Removes the AppListItemView at |index| in |view_model_|, removes it from
  // view structure as well and deletes it.
  void DeleteItemViewAtIndex(int index);

  // Returns true if |point| lies within the bounds of this grid view plus a
  // buffer area surrounding it that can trigger drop target change.
  bool IsPointWithinDragBuffer(const gfx::Point& point) const;

  // Schedules a Layout() call. If `previous_grid_size` is different from the
  // current grid size, calls PreferredSizeChanged().
  void ScheduleLayout(const gfx::Size& previous_grid_size);

  // Overridden from AppListItemListObserver:
  void OnListItemAdded(size_t index, AppListItem* item) override;
  void OnListItemRemoved(size_t index, AppListItem* item) override;
  void OnListItemMoved(size_t from_index,
                       size_t to_index,
                       AppListItem* item) override;

  // Overridden from AppListModelObserver:
  void OnAppListModelStatusChanged() override;

  // Animates `drag_icon_proxy_` to drop it into appropriate target bounds in
  // the apps grid when the item drag ends. Expects `drag_icon_proxy_` to be
  // set.
  // `drag_item` - The dragged item.
  // `target_folder_id` - If the item needs to be dropped into a folder, the
  // target folder ID.
  void AnimateDragIconToTargetPosition(AppListItem* drag_item,
                                       const std::string& target_folder_id);

  // Called when the `drag_icon_proxy_` animation started by
  // `AnimateDragIconToTargetPosition()` finishes. It resets `drag_icon_proxy_`
  // and shows the view that was hidden for drag.
  void OnDragIconDropDone();

  // Returns true if the dragged item isn't a folder, the drag is not
  // occurring inside a folder, and |drop_target_| is a valid index.
  bool DraggedItemCanEnterFolder();

  // Returns the slot number which the given |point| falls into or the closest
  // slot if |point| is outside the page's bounds.
  GridIndex GetNearestTileIndexForPoint(const gfx::Point& point) const;

  // Gets the item view currently displayed at |slot| on the current page. If
  // there is no item displayed at |slot|, returns nullptr. Note that this finds
  // an item *displayed* at a slot, which may differ from the item's location in
  // the model (as it may have been temporarily moved during a drag operation).
  AppListItemView* GetViewDisplayedAtSlotOnCurrentPage(int slot) const;

  // Sets state of the view with |target_index| to |is_target_folder| for
  // dropping |drag_view_|.
  void SetAsFolderDroppingTarget(const GridIndex& target_index,
                                 bool is_target_folder);

  // Invoked when |reorder_timer_| fires to show re-order preview UI.
  void OnReorderTimer();

  // Invoked when |folder_item_reparent_timer_| fires.
  void OnFolderItemReparentTimer(Pointer pointer);

  // Updates drag state for dragging inside a folder's grid view.
  // `pointer` - The pointer that's used for dragging (mouse or touch).
  void UpdateDragStateInsideFolder(Pointer pointer,
                                   const gfx::Point& drag_point);

  // Returns true if drag event is happening in the root level AppsGridView
  // for reparenting a folder item.
  bool IsDraggingForReparentInRootLevelGridView() const;

  // Returns true if drag event is happening in the hidden AppsGridView of the
  // folder during reparenting a folder item.
  bool IsDraggingForReparentInHiddenGridView() const;

  // Returns the target icon bounds for |drag_item| to fly back to its parent
  // |folder_item_view| in animation.
  gfx::Rect GetTargetIconRectInFolder(AppListItem* drag_item,
                                      AppListItemView* folder_item_view);

  // Returns true if the grid view is under an OEM folder.
  bool IsUnderOEMFolder();

  // Handles keyboard app reordering, foldering, and reparenting. Operations
  // effect |selected_view_|. |folder| is whether to move the app into or out of
  // a folder.
  void HandleKeyboardAppOperations(ui::KeyboardCode key_code, bool folder);

  // Handles either creating a folder with |selected_view_| or moving
  // |selected_view_| into an existing folder.
  void HandleKeyboardFoldering(ui::KeyboardCode key_code);

  // Returns whether |selected_view_| can be foldered to the item at
  // |target_index| in the root AppsGridView.
  bool CanMoveSelectedToTargetForKeyboardFoldering(
      const GridIndex& target_index) const;

  // Handle vertical focus movement triggered by arrow up and down.
  bool HandleVerticalFocusMovement(bool arrow_up);

  // Update number of columns and rows for apps within a folder.
  void UpdateColsAndRowsForFolder();

  // Returns true if the visual index is valid position to which an item view
  // can be moved.
  bool IsValidReorderTargetIndex(const GridIndex& index) const;

  // Returns model index of the item view of the specified item.
  int GetModelIndexOfItem(const AppListItem* item) const;

  // Returns the target model index based on item index. (Item index is the
  // index of an item in item list.) This should be used when the item is
  // updated in item list but its item view has not been updated in view model.
  int GetTargetModelIndexFromItemIndex(size_t item_index);

  // Returns the target GridIndex for a keyboard move.
  GridIndex GetTargetGridIndexForKeyboardMove(ui::KeyboardCode key_code) const;

  // Returns the target GridIndex to move an item from a folder to the root
  // AppsGridView.
  // `folder_index` - the grid index of the folder from which an item is
  // reparented.
  GridIndex GetTargetGridIndexForKeyboardReparent(
      const GridIndex& folder_index,
      ui::KeyboardCode key_code) const;

  // Swaps |selected_view_| with the item in relative position specified by
  // |key_code|.
  void HandleKeyboardMove(ui::KeyboardCode key_code);

  // During an app drag, creates an a11y event to verbalize dropping onto a
  // folder or creating a folder with two apps.
  void MaybeCreateFolderDroppingAccessibilityEvent();

  // During an app drag, creates an a11y event to verbalize drop target
  // location.
  void MaybeCreateDragReorderAccessibilityEvent();

  // Modifies the announcement view to verbalize |target_index|.
  void AnnounceReorder(const GridIndex& target_index);

  // Creates a new GhostImageView at |reorder_placeholder_| and initializes
  // |current_ghost_view_| and |last_ghost_view_|.
  void CreateGhostImageView();

  // Invoked when |host_drag_start_timer_| fires.
  void OnHostDragStartTimerFired();

  // Called at the end of the fade out animation. `callback` comes from the
  // caller that starts the fade out animation. `aborted` is true when the fade
  // out animation gets aborted.
  void OnFadeOutAnimationEnded(ReorderAnimationCallback callback, bool aborted);

  // Called at the end of the fade out animation. `callback` comes from the
  // caller that starts the fade in animation. `aborted` is true when the fade
  // in animation gets aborted.
  void OnFadeInAnimationEnded(ReorderAnimationCallback callback, bool aborted);

  // Runs the animation callback popped from the test callback queue if the
  // queue is not empty. The parameters indicate the animation running result
  // and should be passed to the callback.
  void MaybeRunNextReorderAnimationCallbackForTest(
      bool aborted,
      AppListReorderAnimationStatus animation_source);

  // Sets `open_folder_info_` for  a folder that is about to be shown.
  // `folder_id` is the folder's item ID, `target_folder_position` is the grid
  // index at which the folder is located (or being created).
  // `position_to_skip`, if valid, is the grid index of an app list item that is
  // expected to be removed (for example, the reorder placeholder gets removed
  // after an app list item drag ends, and should thus be ignored when
  // calculating the final folder position during drag end). The target folder
  // position should be adjusted as if the item at this position is gone.
  void SetOpenFolderInfo(const std::string& folder_id,
                         const GridIndex& target_folder_position,
                         const GridIndex& position_to_skip);

  // Requsets a folder view for the provided app list folder item to be shown.
  // `new_folder` indicates whether the folder is a newly created folder.
  void ShowFolderForView(AppListItemView* folder_view, bool new_folder);

  // Called when a folder view that was opened from this apps grid hides (and
  // completes hide animation), either when user closes the folder, or when the
  // folder gets hidden during reparent drag.
  // `item_id` is the folder items app list model ID.
  void FolderHidden(const std::string& item_id);

  // When folder item view position in the grid changes while the folder view
  // was shown for the item, the folder item view animates out in old location,
  // and animate in in the new location upon folder view closure. This methods
  // schedules folder item fade-in animation, and schedule bounds animations for
  // other item views in the grid.
  void AnimateFolderItemViewIn();

  // Called when the folder view closes, and optional folder item view "position
  // change" animation completes.
  void OnFolderHideAnimationDone();

  class ScopedModelUpdate;

  AppListModel* model_ = nullptr;         // Owned by AppListView.
  AppListItemList* item_list_ = nullptr;  // Not owned.

  // This can be nullptr. Only grid views inside folders have a folder delegate.
  AppsGridViewFolderDelegate* folder_delegate_ = nullptr;

  // Used to request showing a folder UI for a folder item view.
  // May be nullptr if the AppsGridView is never expected to request a folder to
  // be shown. For example, it will be nullptr for folder items grids (which do
  // not support nested folder items).
  AppListFolderController* const folder_controller_;

  AppListA11yAnnouncer* const a11y_announcer_;
  AppListViewDelegate* const app_list_view_delegate_;

  // May be nullptr if this apps grid doesn't have custom focus handling.
  AppsGridViewFocusDelegate* const focus_delegate_;

  // Keeps the individual AppListItemView. Owned by views hierarchy.
  views::View* items_container_ = nullptr;

  // The `AppListConfig` currently used for sizing app list item views within
  // the grid.
  const AppListConfig* app_list_config_ = nullptr;

  // The max number of columns the grid can have.
  int max_cols_ = 0;

  int cols_ = 0;

  // List of app item views. There is a view per item in |model_|.
  views::ViewModelT<AppListItemView> view_model_;

  // List of pulsing block views.
  views::ViewModelT<PulsingBlockView> pulsing_blocks_model_;

  AppListItemView* selected_view_ = nullptr;

  // Set while the AppsGridView is handling drag operation for an app list item.
  // It's set to the drag item that is being dragged in the UI. If `drag_view_`
  // is set, it should have the same value as `drag_view_->item()`.
  AppListItem* drag_item_ = nullptr;

  // The index of the drag_view_ when the drag starts.
  GridIndex drag_view_init_index_;

  // The point where the drag started in GridView coordinates.
  gfx::Point drag_start_grid_view_;

  Pointer drag_pointer_ = NONE;

  // Whether the apps grid is currently updating the app list model.
  bool updating_model_ = false;

  // Object that while in scope hides the drag view from the UI during the drag
  // operation. Note that this may remain set even after ClearDragState(), while
  // the drag icon proxy animation is in progress.
  std::unique_ptr<DragViewHider> drag_view_hider_;

  // Object that while scope keeps an app list item icon hidden from a folder
  // view icon. Used to hide a drag item icon from a folder icon while the item
  // is being dropped into the folder.
  std::unique_ptr<FolderIconItemHider> folder_icon_item_hider_;

  // The most recent reorder drop target.
  GridIndex drop_target_;

  // The most recent folder drop target.
  GridIndex folder_drop_target_;

  // The index where an empty slot has been left as a placeholder for the
  // reorder drop target. This updates when the reorder animation triggers.
  GridIndex reorder_placeholder_;

  // The current action that ending a drag will perform.
  DropTargetRegion drop_target_region_ = NO_TARGET;

  // Timer for re-ordering the |drop_target_region_| and |drag_view_|.
  base::OneShotTimer reorder_timer_;

  // Timer for dragging a folder item out of folder container ink bubble.
  base::OneShotTimer folder_item_reparent_timer_;

  // Timer for |drag_and_drop_host_| to start handling drag operations.
  base::OneShotTimer host_drag_start_timer_;

  // An application target drag and drop host which accepts dnd operations.
  // Usually the shelf (e.g. ShelfView or ScrollableShelfView).
  ApplicationDragAndDropHost* drag_and_drop_host_ = nullptr;

  // The drag operation is currently inside the dnd host and events get
  // forwarded.
  bool forward_events_to_drag_and_drop_host_ = false;

  // Last mouse drag location in this view's coordinates.
  gfx::Point last_drag_point_;

  // Used to animate individual icon positions.
  std::unique_ptr<views::BoundsAnimator> bounds_animator_;

  // Tracks if drag_view_ is dragged out of the folder container bubble
  // when dragging a item inside a folder.
  bool drag_out_of_folder_container_ = false;

  // Whether a sequence of keyboard moves are happening.
  bool handling_keyboard_move_ = false;

  // True if the drag_view_ item is a folder item being dragged for reparenting.
  bool dragging_for_reparent_item_ = false;

  // The folder that should be opened after drag icon drop animation finishes.
  // This is set when an item drag ends in a folder creation, in which case the
  // created folder is expected to open after drag (assuming productivity
  // launcher feature is enabled).
  std::string folder_to_open_after_drag_icon_animation_;

  // When dragging for reparent in the root view, a callback registered by the
  // originating, hidden grid that when called will cancel drag operation in the
  // hidden view. Used in cases the root grid detects that the drag should end,
  // for example due to app list model changes.
  base::OnceClosure reparent_drag_cancellation_;

  // The drop location of the most recent reorder related accessibility event.
  GridIndex last_reorder_a11y_event_location_;

  // The location of the most recent foldering drag related accessibility event.
  GridIndex last_folder_dropping_a11y_event_location_;

  // The location when |current_ghost_view_| was shown.
  GridIndex current_ghost_location_;

  struct OpenFolderInfo {
    std::string item_id;
    GridIndex grid_index;
  };
  // Set when a folder view was opened from the apps grid - it contains the
  // opened folder ID and original location in the grid. While the folder
  // remains open, the folder item view position will be forced to the original
  // grid slot, to prevent folder UI from jumping, or empty slots from appearing
  // behind a folder when the gird item list changes (e.g. if another item gets
  // added by sync, or the folder item moves as a result of folder rename).
  absl::optional<OpenFolderInfo> open_folder_info_;

  // Folder item view that is being animated into it's target position. The
  // animation runs after a folder gets closed if the folder intended position
  // in the grid changed while the folder was open.
  absl::optional<views::View*> reordering_folder_view_;

  std::unique_ptr<AppsGridContextMenu> context_menu_;

  // Indicates the current reorder animation.
  AppListReorderAnimationStatus reorder_animation_status_ =
      AppListReorderAnimationStatus::kEmpty;

  // A handle that aborts the active reorder animation.
  std::unique_ptr<views::AnimationAbortHandle> reorder_animation_abort_handle_;

  // If false, the animation to move an app list item when the item's target
  // position changes is disabled. It is set to be false when we only care about
  // app list items' final positions instead of animation process.
  bool enable_item_move_animation_ = true;

  // Tracks the reorder animation triggered by the sort order change.
  absl::optional<ui::ThroughputTracker> reorder_animation_tracker_;

  // A queue of callbacks that run at the end of app list reorder. A reorder
  // ends if:
  // (1) Fade out animation is aborted, or
  // (2) Fade in animation is aborted or ends normally.
  std::queue<TestReorderDoneCallbackType>
      reorder_animation_callback_queue_for_test_;

  // A closure that runs at the start of the fade out animation.
  base::OnceClosure fade_out_start_closure_for_test_;

  // A closure that runs at the end of the fade out animation.
  base::OnceClosure fade_out_done_closure_for_test_;

  base::WeakPtrFactory<AppsGridView> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_APPS_GRID_VIEW_H_
