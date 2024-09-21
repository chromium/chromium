// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_APPS_GRID_VIEW_H_
#define ASH_APP_LIST_VIEWS_APPS_GRID_VIEW_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <optional>
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
#include "ash/app_list/views/app_list_item_view.h"
#include "ash/app_list/views/app_list_item_view_grid_delegate.h"
#include "ash/ash_export.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/compositor/throughput_tracker.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/views/animation/animation_abort_handle.h"
#include "ui/views/view.h"
#include "ui/views/view_model.h"

namespace ash {

namespace test {
class AppsGridViewTest;
class AppsGridViewTestApi;
}  // namespace test

class AppListA11yAnnouncer;
class ApplicationDragAndDropHost;
class AppListConfig;
class AppListFolderController;
class AppListItem;
class AppListItemList;
class AppListItemView;
class AppListKeyboardController;
class AppListModel;
class AppListViewDelegate;
class AppsGridContextMenu;
class AppsGridViewFolderDelegate;
class PulsingBlockView;
class AppsGridRowChangeAnimator;
class GhostImageView;
class AppsGridViewTest;
class ScrollableAppsGridViewTest;
class PagedAppsGridViewTest;

// AppsGridView displays a grid of app icons. It is used for:
// - The main grid of apps in the launcher
// - The grid of apps in a folder
class ASH_EXPORT AppsGridView : public views::View,
                                public AppListItemViewGridDelegate,
                                public AppListItemListObserver,
                                public AppListItemObserver,
                                public AppListModelObserver {
  METADATA_HEADER(AppsGridView, views::View)

 public:
  enum Pointer {
    NONE,
    MOUSE,
    TOUCH,
  };

  AppsGridView(AppListA11yAnnouncer* a11y_announcer,
               AppListViewDelegate* app_list_view_delegate,
               AppsGridViewFolderDelegate* folder_delegate,
               AppListFolderController* folder_controller,
               AppListKeyboardController* keyboard_controller);
  AppsGridView(const AppsGridView&) = delete;
  AppsGridView& operator=(const AppsGridView&) = delete;
  ~AppsGridView() override;

  using PendingAppsMap = std::map<std::string, ui::ImageModel>;

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
  void EndDrag(bool cancel) override;
  void OnAppListItemViewActivated(AppListItemView* pressed_item_view,
                                  const ui::Event& event) override;
  bool IsAboveTheFold(AppListItemView* item_view) override;

  bool IsDragging() const;
  bool IsDraggedView(const AppListItemView* view) const;

  void ClearDragState();

  // Set the drag and drop host for application links.
  void SetDragAndDropHostOfCurrentAppList(
      ApplicationDragAndDropHost* drag_and_drop_host);

  // Return true if `view` is undergoing a layer animation.
  bool IsAnimatingView(AppListItemView* view) const;

  const AppListConfig* app_list_config() const { return app_list_config_; }

  bool has_selected_view() const { return selected_view_ != nullptr; }
  AppListItemView* selected_view() const { return selected_view_; }

  const AppListItemView* drag_view() const { return drag_view_; }

  bool has_dragged_item() const { return drag_item_ != nullptr; }
  const AppListItem* drag_item() const { return drag_item_; }

  // Overridden from views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  bool OnKeyReleased(const ui::KeyEvent& event) override;
  void ViewHierarchyChanged(
      const views::ViewHierarchyChangedDetails& details) override;
  bool GetDropFormats(int* formats,
                      std::set<ui::ClipboardFormatType>* format_types) override;
  bool CanDrop(const OSExchangeData& data) override;
  int OnDragUpdated(const ui::DropTargetEvent& event) override;
  void OnDragEntered(const ui::DropTargetEvent& event) override;
  void OnDragExited() override;
  DropCallback GetDropCallback(const ui::DropTargetEvent& event) override;

  // Whether or not the apps grid should handle drag and drop events or delegate
  // them to the container.
  virtual bool ShouldContainerHandleDragEvents() = 0;

  // Whether the apps grid will accept the drop event by the data it carries.
  bool WillAcceptDropEvent(const OSExchangeData& data);

  // Updates the visibility of app list items according to |app_list_state|.
  void UpdateControlVisibility(AppListViewState app_list_state);

  // Returns the item view of the item with the provided item ID.
  // Returns nullptr if there is no such item.
  AppListItemView* GetItemViewForItem(const std::string& item_id);

  // Returns the item view of the item at |index|, or nullptr if there is no
  // view at |index|.
  AppListItemView* GetItemViewAt(size_t index) const;

  // Updates drag in the root level grid view when receiving the drag event
  // dispatched from the hidden grid view for reparenting a folder item.
  // `pointer` - The pointer that's used for dragging (mouse or touch).
  void UpdateDragFromReparentItem(Pointer pointer,
                                  const gfx::Point& drag_point);

  // Handles EndDrag event dispatched from the hidden folder grid view in the
  // root level grid view to end reparenting a folder item.
  // |original_parent_item_view|: The folder AppListView for the folder from
  // which drag item is being dragged.
  // |events_forwarded_to_drag_drop_host|: True if the dragged item is dropped
  // to the drag_drop_host, eg. dropped on shelf.
  // |cancel_drag|: True if the drag is ending because it has been canceled.
  void EndDragFromReparentItemInRootLevel(
      AppListItemView* original_parent_item_view,
      bool events_forwarded_to_drag_drop_host,
      bool cancel_drag);

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

  // Slides visible items up when the continue section is hidden in tablet mode.
  // Each row of items has a different vertical offset, creating a "cascade"
  // effect. `base_offset` is the offset for the first row. Subsequent rows have
  // a smaller offset.
  void SlideVisibleItemsForHideContinueSection(int base_offset);

  // Whether the provided view is hidden to facilitate drag operation (for
  // example, the drag view for which a drag icon proxy has been created).
  bool IsViewHiddenForDrag(const views::View* view) const;

  // Whether `view` is the folder view that is animating out and in as part of
  // folder reorder animation that runs after folder is closed if the folder
  // position within the grid changed.
  bool IsViewHiddenForFolderReorder(const views::View* view) const;

  // Returns true if the whole apps grid is animating (for reordering, or hide
  // continue section). This function is public for testing.
  bool IsUnderWholeGridAnimation() const;

  // Returns whether `view` is hidden due to drag or folder reorder.
  bool IsViewExplicitlyHidden(const views::View* view) const;

  // Aborts the active whole-grid animation (for reordering, or hide continue
  // section), if any.
  void MaybeAbortWholeGridAnimation();

  // Passes scroll information from a parent view, so that subclasses may scroll
  // or switch pages.
  virtual void HandleScrollFromParentView(const gfx::Vector2d& offset,
                                          ui::EventType type) = 0;

  // Return the view model.
  views::ViewModelT<AppListItemView>* view_model() { return &view_model_; }
  const views::ViewModelT<AppListItemView>* view_model() const {
    return &view_model_;
  }

  // Returns whether any item view is currently animating.
  bool IsItemAnimationRunning() const;

  // Stop animations for all item views in `view_model_`.
  void CancelAllItemAnimations();

  AppsGridViewFolderDelegate* folder_delegate() const {
    return folder_delegate_;
  }

  void set_folder_delegate(AppsGridViewFolderDelegate* folder_delegate) {
    folder_delegate_ = folder_delegate;
  }

  const AppListModel* model() const { return model_; }

  GridIndex reorder_placeholder() const { return reorder_placeholder_; }

  AppsGridView::Pointer drag_pointer() const { return drag_pointer_; }

  bool FireDragToShelfTimerForTest();

  // Carries two parameters:
  // (1) A boolean value that is true if the reorder is aborted.
  // (2) An enum that specifies the animation stage when the done callback runs.
  using TestReorderDoneCallbackType =
      base::RepeatingCallback<void(bool aborted,
                                   AppListGridAnimationStatus status)>;

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

  // Set `view` as hidden for testing, similar to when a view is hidden during
  // drag or a sorted folder is renamed and hidden during reorder.
  void set_hidden_view_for_test(views::View* view) {
    hidden_view_for_test_ = view;
  }

  base::OneShotTimer* reorder_timer_for_test() { return &reorder_timer_; }

  AppsGridContextMenu* context_menu_for_test() { return context_menu_.get(); }

  void set_enable_item_move_animation_for_test(bool enable) {
    enable_item_move_animation_ = enable;
  }

  AppListGridAnimationStatus grid_animation_status_for_test() const {
    return grid_animation_status_;
  }

  ui::Layer* drag_image_layer_for_test() const {
    if (!drag_image_layer_) {
      return nullptr;
    }
    return drag_image_layer_->root();
  }

 protected:
  friend AppsGridViewTest;
  friend ScrollableAppsGridViewTest;

  struct VisibleItemIndexRange {
    VisibleItemIndexRange();
    VisibleItemIndexRange(size_t first_index, size_t last_index);
    ~VisibleItemIndexRange();

    // The view index of the first visible item on the apps grid.
    size_t first_index = 0;

    // The view index of the last visible item on the apps grid.
    size_t last_index = 0;
  };

  // The duration in ms for most of the apps grid view animations.
  static constexpr int kDefaultAnimationDuration = 200;

  // Returns the size of a tile view excluding its padding.
  virtual gfx::Size GetTileViewSize() const = 0;

  // Returns the padding around a tile view.
  virtual gfx::Insets GetTilePadding(int page) const = 0;

  // Returns the size of the entire tile grid.
  virtual gfx::Size GetTileGridSize() const = 0;

  // Returns the max number of rows the grid can have on a page. Returns nullopt
  // if apps grid does not have limit on number of rows (which currently implies
  // scrollable, single-page apps grid).
  virtual std::optional<int> GetMaxRowsInPage(int page) const = 0;

  // Calculates the offset distance to center the grid in the container.
  virtual gfx::Vector2d GetGridCenteringOffset(int page) const = 0;

  // Returns number of total pages, or one if the grid does not use pages.
  virtual int GetTotalPages() const = 0;

  // Returns the current selected page, or zero if the grid does not use pages.
  virtual int GetSelectedPage() const = 0;

  // Returns whether the page at `page_index` is full (and no more app list
  // items can be appended to the page).
  virtual bool IsPageFull(size_t page_index) const = 0;

  // Give an item index in the apps grid view model, returns the item's grid
  // index (the page the items belongs to, and the item index within that page).
  virtual GridIndex GetGridIndexFromIndexInViewModel(int index) const = 0;

  // Returns the number of pulsing block views should be added to the grid given
  // the number of items in the app list model. Pulsing blocks are added to the
  // app list during initial app list model sync to indicate the app list is
  // still syncing/finalizing. They get appended to the end of the app list.
  // This method will only get called when app list model is syncing (i.e. in
  // state that calls for pulsing blocks).
  virtual int GetNumberOfPulsingBlocksToShow(int item_count) const = 0;

  // Records the different ways to move an app in app list's apps grid for UMA
  // histograms.
  virtual void RecordAppMovingTypeMetrics(AppListAppMovingType type) = 0;

  // Starts the "cardified" state if the subclass supports it.
  virtual void MaybeStartCardifiedView() {}

  // Ends the "cardified" state if the subclass supports it.
  virtual void MaybeEndCardifiedView() {}

  // Ends the "cardified" state if the subclass supports it.
  virtual bool IsAnimatingCardifiedState() const;

  // Starts a page flip if the subclass supports it.
  virtual bool MaybeStartPageFlip();

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
  virtual void SetFocusAfterEndDrag(AppListItem* drag_item) = 0;

  // Calculates the index range of the visible item views.
  virtual std::optional<VisibleItemIndexRange> GetVisibleItemIndexRange()
      const = 0;

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
  void SetIdealBoundsForViewToGridIndex(size_t view_index_in_model,
                                        const GridIndex& view_grid_index);

  // Calculates the item views' bounds for both folder and non-folder.
  void CalculateIdealBounds();
  void CalculateIdealBoundsForPageStructureWithPartialPages();

  // Gets the bounds of the tile located at |index|, where |index| contains the
  // page/slot info.
  gfx::Rect GetExpectedTileBounds(const GridIndex& index) const;

  // Returns the number of app tiles per page. Takes a page number as an
  // argument as the first page might have less apps shown.
  // Returns nullopt if number of tiles per page is not limited (which currently
  // implies scrollable, single-page apps grid).
  std::optional<int> TilesPerPage(int page) const;

  // Converts an app list item position in app list grid to its index in the
  // apps grid `view_model_`.
  int GetIndexInViewModel(const GridIndex& index) const;

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

  // Destroys all item view layers if they are not required.
  void DestroyLayerItemsIfNotNeeded();

  // Whether app list item views require layers - for example during drag, or
  // folder repositioning animation.
  bool ItemViewsRequireLayers() const;

  void BeginHideCurrentGhostImageView();

  // Ensures layer for all app items before animations are started.
  void PrepareItemsForBoundsAnimation();

  // Whether the apps grid has an extra slot, in addition to slots for views in
  // `view_model_`, specially for drag item placeholder. Generally, the
  // placeholder will take the hidden drag view's slot, but during reparent
  // drag, the target apps grid view model may not contain a view for the drag
  // item. In this case the placeholder will have it's own grid slot.
  bool HasExtraSlotForReorderPlaceholder() const;

  bool ignore_layout() const { return ignore_layout_; }
  views::View* items_container() { return items_container_; }
  const views::ViewModelT<PulsingBlockView>& pulsing_blocks_model() const {
    return pulsing_blocks_model_;
  }
  const gfx::Point& last_drag_point() const { return last_drag_point_; }
  void set_last_drag_point(const gfx::Point& p) { last_drag_point_ = p; }

  AppListViewDelegate* app_list_view_delegate() const {
    return app_list_view_delegate_;
  }
  const AppListItemList* item_list() const { return item_list_; }

  // The `AppListItemView` that is being dragged within the apps grid (i.e. the
  // AppListItemView for `drag_item_`) if the drag item is currently part of the
  // item list shown in the apps grid. `drag_view_` may be nullptr during item
  // reparent drag while being handled in the root app list grid (the drag item
  // will be added to target item list only when the drag ends).
  // Subclasses need non-const access.
  raw_ptr<AppListItemView> drag_view_ = nullptr;

  // If true, layout does nothing. See where set for details.
  bool ignore_layout_ = false;

  // True if the AppList is in cardified state. "Cardified" means showing a
  // rounded rectangle background "card" behind each page during a drag. The
  // grid icons are reduced in size in this state.
  // TODO(crbug.com/40182999): Move cardified state members to
  // PagedAppsGridView.
  bool cardified_state_ = false;

  // Tile spacing between the tile views.
  int horizontal_tile_padding_ = 0;
  int vertical_tile_padding_ = 0;

  // Whether tile padding within the apps grid is fixed.
  bool has_fixed_tile_padding_ = false;

  // True if an extra page is opened after the user drags an app to the bottom
  // of last page with intention to put it in a new page. This is only used for
  // non-folder.
  bool extra_page_opened_ = false;

  raw_ptr<GhostImageView> current_ghost_view_ = nullptr;
  raw_ptr<GhostImageView> last_ghost_view_ = nullptr;

 private:
  friend class test::AppsGridViewTestApi;
  friend class test::AppsGridViewTest;
  friend class PagedAppsGridView;
  friend class AppsGridRowChangeAnimator;
  friend class PagedAppsGridViewTest;

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
  // animates their bounds using layer transform.
  // `is_animating_top_to_bottom` - Whether the ideal bounds animation will
  // initialize starting from the top items or bottom items. The result is used
  // to determine whether item animation duration grows top to bottom or bottom
  // to top.
  void AnimateToIdealBounds(bool is_animating_top_to_bottom);

  // Whether the ideal bounds animation will happen across multiple rows.
  bool WillAnimateMultipleRows();

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

  // Records the user metrics action for deleting a folder if `folder_id` has
  // been removed from the data model. Called after an app item move which might
  // or might not cause a folder to be deleted.
  void MaybeRecordFolderDeleteUserAction(const std::string& folder_id);

  // Removes the AppListItemView at |index| in |view_model_|, removes it from
  // view structure as well and deletes it.
  void DeleteItemViewAtIndex(size_t index);

  // Returns true if |point| lies within the bounds of this grid view plus a
  // buffer area surrounding it that can trigger drop target change.
  bool IsPointWithinDragBuffer(const gfx::Point& point) const;

  // Schedules a layout. If `previous_grid_size` is different from the current
  // grid size, calls PreferredSizeChanged().
  void ScheduleLayout(const gfx::Size& previous_grid_size);

  // Overridden from AppListItemListObserver:
  void OnListItemAdded(size_t index, AppListItem* item) override;
  void OnListItemRemoved(size_t index, AppListItem* item) override;
  void OnListItemMoved(size_t from_index,
                       size_t to_index,
                       AppListItem* item) override;

  // AppListItemObserver:
  void ItemBeingDestroyed() override;

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
  size_t GetModelIndexOfItem(const AppListItem* item) const;

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

  // Called at the end of the fade out animation. `callback` comes from the
  // caller that starts the fade out animation. `aborted` is true when the fade
  // out animation gets aborted.
  void OnFadeOutAnimationEnded(ReorderAnimationCallback callback, bool aborted);

  // Called at the end of the fade out animation. `callback` comes from the
  // caller that starts the fade in animation. `aborted` is true when the fade
  // in animation gets aborted.
  void OnFadeInAnimationEnded(ReorderAnimationCallback callback, bool aborted);

  // Called at the end of the hide continue section animation.
  void OnHideContinueSectionAnimationEnded();

  // Runs the animation callback popped from the test callback queue if the
  // queue is not empty. The parameters indicate the animation running result
  // and should be passed to the callback.
  void MaybeRunNextReorderAnimationCallbackForTest(
      bool aborted,
      AppListGridAnimationStatus animation_source);

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

  // Called when ideal bounds animations complete.
  void OnIdealBoundsAnimationDone();

  // Callback method to clean up the dragging state of the app list.
  void EndDragCallback(
      const ui::DropTargetEvent& event,
      ui::mojom::DragOperation& output_drag_op,
      std::unique_ptr<ui::LayerTreeOwner> drag_image_layer_owner);

  // Registers a pending promise app removal - called when a promise app item is
  // removed due to a successful app installation. It adds the promise package
  // ID to the pending promise app removal set, and caches the promise app
  // image, which will be used as a default/fallback image for the app during
  // the animation to replace the promise app with the installed app.
  void AddPendingPromiseAppRemoval(const std::string& id,
                                   const ui::ImageModel& promise_app_image);

  // Called when the transition animation between apps is done.
  void FinishAnimationForPromiseApps(const std::string& pending_app_id);

  // Duplicates the layer for the `promise_app_view` and adds it to
  // `pending_promise_apps_removals_` if an animation would be required for it
  // on the future.
  void MaybeDuplicatePromiseAppForRemoval(AppListItemView* promise_app_view);

  class ScopedModelUpdate;

  raw_ptr<AppListModel> model_ = nullptr;         // Owned by AppListView.
  raw_ptr<AppListItemList> item_list_ = nullptr;  // Not owned.

  // This can be nullptr. Only grid views inside folders have a folder delegate.
  raw_ptr<AppsGridViewFolderDelegate> folder_delegate_ = nullptr;

  // Used to request showing a folder UI for a folder item view.
  // May be nullptr if the AppsGridView is never expected to request a folder to
  // be shown. For example, it will be nullptr for folder items grids (which do
  // not support nested folder items).
  const raw_ptr<AppListFolderController> folder_controller_;

  const raw_ptr<AppListA11yAnnouncer, DanglingUntriaged> a11y_announcer_;
  const raw_ptr<AppListViewDelegate> app_list_view_delegate_;

  // May be nullptr if this apps grid doesn't have custom focus handling, for
  // example, a folder apps grid.
  const raw_ptr<AppListKeyboardController, DanglingUntriaged>
      keyboard_controller_;

  // Keeps the individual AppListItemView. Owned by views hierarchy.
  raw_ptr<views::View, DanglingUntriaged> items_container_ = nullptr;

  // The `AppListConfig` currently used for sizing app list item views within
  // the grid.
  raw_ptr<const AppListConfig, DanglingUntriaged> app_list_config_ = nullptr;

  // The max number of columns the grid can have.
  int max_cols_ = 0;

  int cols_ = 0;

  // List of app item views. There is a view per item in |model_|.
  views::ViewModelT<AppListItemView> view_model_;

  // List of pulsing block views.
  views::ViewModelT<PulsingBlockView> pulsing_blocks_model_;

  raw_ptr<AppListItemView> selected_view_ = nullptr;

  // Set while the AppsGridView is handling drag operation for an app list item.
  // It's set to the drag item that is being dragged in the UI. If `drag_view_`
  // is set, it should have the same value as `drag_view_->item()`.
  raw_ptr<AppListItem> drag_item_ = nullptr;

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

  // Last mouse drag location in this view's coordinates.
  gfx::Point last_drag_point_;

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

  // The layer that contains the icon image for the item under the drag cursor.
  // Assigned before the dropping animation is scheduled. Not owned.
  std::unique_ptr<ui::LayerTreeOwner> drag_image_layer_;

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
  std::optional<OpenFolderInfo> open_folder_info_;

  // Folder item view that is being animated into it's target position. The
  // animation runs after a folder gets closed if the folder intended position
  // in the grid changed while the folder was open.
  std::optional<AppListItemView*> reordering_folder_view_;

  // A view which is hidden for testing purposes.
  raw_ptr<views::View, DanglingUntriaged> hidden_view_for_test_ = nullptr;

  std::unique_ptr<AppsGridContextMenu> context_menu_;

  // The status of any whole-grid animation (reorder, hide continue section).
  AppListGridAnimationStatus grid_animation_status_ =
      AppListGridAnimationStatus::kEmpty;

  // A handle that aborts the active whole-grid animation.
  std::unique_ptr<views::AnimationAbortHandle> grid_animation_abort_handle_;

  // If false, the animation to move an app list item when the item's target
  // position changes is disabled. It is set to be false when we only care about
  // app list items' final positions instead of animation process.
  bool enable_item_move_animation_ = true;

  // Tracks the reorder animation triggered by the sort order change.
  std::optional<ui::ThroughputTracker> reorder_animation_tracker_;

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

  // Used to trigger and manage row change animations.
  std::unique_ptr<AppsGridRowChangeAnimator> row_change_animator_;

  // Tracks the animation smoothness of item reorders during drag. Gets
  // triggered by AnimateToIdealBounds(), which is mainly caused
  // by app dragging reorders. This does not track reorders due to sort.
  std::optional<ui::ThroughputTracker> item_reorder_animation_tracker_;

  // Whether an ideal bounds animation is being setup. Used to prevent item
  // layers from being deleted during setup.
  bool setting_up_ideal_bounds_animation_ = false;

  // A list of pending promise app layers to be removed when the actual app is
  // pushed into the apps grid.
  PendingAppsMap pending_promise_apps_removals_;

  base::WeakPtrFactory<AppsGridView> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_APPS_GRID_VIEW_H_
