// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_APPS_CONTAINER_VIEW_H_
#define ASH_APP_LIST_VIEWS_APPS_CONTAINER_VIEW_H_

#include <stddef.h>

#include <memory>

#include "ash/app_list/app_list_model_provider.h"
#include "ash/app_list/model/app_list_folder_item.h"
#include "ash/app_list/views/app_list_folder_controller.h"
#include "ash/app_list/views/app_list_nudge_controller.h"
#include "ash/app_list/views/app_list_page.h"
#include "ash/app_list/views/app_list_toast_container_view.h"
#include "ash/app_list/views/paged_apps_grid_view.h"
#include "ash/app_list/views/recent_apps_view.h"
#include "ash/app_list/views/search_result_page_dialog_controller.h"
#include "ash/ash_export.h"
#include "ash/public/cpp/pagination/pagination_model_observer.h"
#include "base/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "ui/views/controls/separator.h"

namespace ash {

class ApplicationDragAndDropHost;
class AppListFolderItem;
class AppListFolderView;
class AppListNudgeController;
class ContentsView;
class ContinueSectionView;
class FolderBackgroundView;
class PageSwitcher;
class SearchResultPageAnchoredDialog;
class SuggestionChipContainerView;
class GradientLayerDelegate;

// AppsContainerView contains a root level AppsGridView to render the root level
// app items, and a AppListFolderView to render the app items inside the
// active folder.
class ASH_EXPORT AppsContainerView
    : public AppListPage,
      public AppListModelProvider::Observer,
      public AppListFolderController,
      public PaginationModelObserver,
      public PagedAppsGridView::ContainerDelegate,
      public RecentAppsView::Delegate,
      public AppListToastContainerView::Delegate {
 public:
  explicit AppsContainerView(ContentsView* contents_view);

  AppsContainerView(const AppsContainerView&) = delete;
  AppsContainerView& operator=(const AppsContainerView&) = delete;

  ~AppsContainerView() override;

  // Resets the app list to a state where it shows the main grid view. This is
  // called when the user opens the launcher for the first time or when the user
  // hides and then shows it.
  void ResetForShowApps();

  // Sets |drag_and_drop_host_| for the current app list in both
  // app_list_folder_view_ and root level apps_grid_view_.
  void SetDragAndDropHostOfCurrentAppList(
      ApplicationDragAndDropHost* drag_and_drop_host);

  // Returns true if it is currently showing an active folder page.
  bool IsInFolderView() const;

  // Updates the visibility of the items in this view according to
  // |app_list_state| and |is_in_drag|.
  void UpdateControlVisibility(AppListViewState app_list_state,
                               bool is_in_drag);

  // Called when tablet mode starts and ends.
  void OnTabletModeChanged(bool started);

  // Minimal margin for apps grid within the apps container. Set to ensure there
  // is enough space to fit page switcher next to the apps grid.
  int GetMinHorizontalMarginForAppsGrid() const;

  // The minimal top margin for the apps grids (measured from the top of the
  // apps container). Set to accommodate min apps container margins, search box
  // and suggestion chips.
  // For productivity launcher UI, this will not include space for continue
  // section and recent apps.
  int GetMinTopMarginForAppsGrid(const gfx::Size& search_box_size) const;

  // Returns the ideal margins for content within the apps container. The actual
  // margins may differ depending on available screen real-estate. For example,
  // margins may be smaller if the apps grid contents would not fit within the
  // ideal margins.
  int GetIdealHorizontalMargin() const;
  int GetIdealVerticalMargin() const;

  // Calculates the apps container or apps grid margin depending on the
  // available content bounds, and search box size.
  // |available_bounds| - The bounds available to lay out either full apps
  //      container or apps grid (depending on |for_full_contaier_bounds|).
  // |search_box_size| - The expected search box size. Used to determine the
  //      the amount of space in apps container available to the apps grid
  //      (if calaulating margins for apps grid, |available_bounds| should
  //      not contain the search box, so this value will not be used in that
  //      case).
  //
  // NOTE: This should not call into ContentsView::GetSearchBoxBounds*()
  // methods, as CalculateMarginsForAvailableBounds is used to calculate the
  // search box bounds.
  const gfx::Insets& CalculateMarginsForAvailableBounds(
      const gfx::Rect& available_bounds,
      const gfx::Size& search_box_size);

  // views::View overrides:
  void Layout() override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  const char* GetClassName() const override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  void OnBoundsChanged(const gfx::Rect& old_bounds) override;
  void OnThemeChanged() override;

  // AppListPage overrides:
  void OnShown() override;
  void OnWillBeHidden() override;
  void OnHidden() override;
  void OnAnimationStarted(AppListState from_state,
                          AppListState to_state) override;
  void UpdatePageOpacityForState(AppListState state,
                                 float search_box_opacity,
                                 bool restore_opacity) override;
  void UpdatePageBoundsForState(AppListState state,
                                const gfx::Rect& contents_bounds,
                                const gfx::Rect& search_box_bounds) override;
  gfx::Rect GetPageBoundsForState(
      AppListState state,
      const gfx::Rect& contents_bounds,
      const gfx::Rect& search_box_bounds) const override;
  void AnimateOpacity(float current_progress,
                      AppListViewState target_view_state,
                      const OpacityAnimator& animator) override;
  void AnimateYPosition(AppListViewState target_view_state,
                        const TransformAnimator& animator,
                        float default_offset) override;

  // AppListModelProvider::Observer:
  void OnActiveAppListModelsChanged(AppListModel* model,
                                    SearchModel* search_model) override;

  // AppListFolderController:
  void ShowFolderForItemView(AppListItemView* folder_item_view,
                             bool focus_name_input,
                             base::OnceClosure hide_callback) override;
  void ShowApps(AppListItemView* folder_item_view, bool select_folder) override;
  void ReparentFolderItemTransit(AppListFolderItem* folder_item) override;
  void ReparentDragEnded() override;

  // PaginationModelObserver:
  void SelectedPageChanged(int old_selected, int new_selected) override;
  void TransitionChanged() override;
  void TransitionStarted() override;
  void TransitionEnded() override;
  void ScrollStarted() override;
  void ScrollEnded() override;

  // PagedAppsGridView::ContainerDelegate:
  bool IsPointWithinPageFlipBuffer(const gfx::Point& point) const override;
  bool IsPointWithinBottomDragBuffer(const gfx::Point& point,
                                     int page_flip_zone_size) const override;
  void OnCardifiedStateStarted() override;
  void OnCardifiedStateEnded() override;

  // RecentAppsView::Delegate:
  void MoveFocusUpFromRecents() override;
  void MoveFocusDownFromRecents(int column) override;

  // AppListToastContainerView::Delegate:
  bool MoveFocusUpFromToast(int column) override;
  bool MoveFocusDownFromToast(int column) override;
  void OnNudgeRemoved() override;

  // Handles `AppListController::UpdateAppListWithNewSortingOrder()` for the
  // app list container.
  void UpdateForNewSortingOrder(
      const absl::optional<AppListSortOrder>& new_order,
      bool animate,
      base::OnceClosure update_position_closure,
      base::OnceClosure animation_done_closure);

  // Called when the app list temporary sort order changes. If `new_order` is
  // null, the temporary sort order is cleared.
  void OnTemporarySortOrderChanged(
      const absl::optional<AppListSortOrder>& new_order);

  // Called by app list controller when the app list visibility is about to
  // change - when the app list is about to be shown, initiates zero state
  // search in order to update set of apps shown in recent apps and continue
  // section contents.
  void OnAppListVisibilityWillChange(bool visible);

  // Updates the nudge in `toast_container_` when app list visibility changes.
  void OnAppListVisibilityChanged(bool shown);

  ContinueSectionView* GetContinueSection();
  RecentAppsView* GetRecentApps();
  views::Separator* separator() { return separator_; }
  PagedAppsGridView* apps_grid_view() { return apps_grid_view_; }
  FolderBackgroundView* folder_background_view() {
    return folder_background_view_;
  }
  AppListFolderView* app_list_folder_view() { return app_list_folder_view_; }
  PageSwitcher* page_switcher() { return page_switcher_; }

  views::View* scrollable_container_for_test() { return scrollable_container_; }

  SuggestionChipContainerView* suggestion_chip_container_view_for_test() {
    return suggestion_chip_container_view_;
  }

  AppListToastContainerView* toast_container_for_test() {
    return toast_container_;
  }

  AppListNudgeController* app_list_nudge_controller() {
    return app_list_nudge_controller_.get();
  }

  // Updates recent apps from app list model. `needs_layout` indicates whether
  // the apps container relaid out when the recent apps results are updated.
  void UpdateRecentApps(bool needs_layout);

  // Updates suggestion chips from app list model.
  void UpdateSuggestionChips();

  // Temporarily disables blur on suggestion chips view background. The blur
  // will remained disabled until the returned closure runner goes out of scope.
  base::ScopedClosureRunner DisableSuggestionChipsBlur();

  // Gets the height of the `separator_` including its vertical margin.
  int GetSeparatorHeight();

  SearchResultPageAnchoredDialog* dialog_for_test() {
    return dialog_controller_->dialog();
  }

 private:
  enum ShowState {
    SHOW_NONE,  // initial state
    SHOW_APPS,
    SHOW_ACTIVE_FOLDER,
    SHOW_ITEM_REPARENT,
  };

  class ContinueContainer;

  void SetShowState(ShowState show_state, bool show_apps_with_animation);

  // Updates the whole container opacity to match the app list state.
  void UpdateContainerOpacityForState(AppListState state);

  // Updates the opacity of the apps container elements for the current app list
  // view position.
  // |progress| - The current app list view drag progress.
  // |restore_opacity| - Whether the opacity should be restored to the non-drag
  //     state.
  void UpdateContentsOpacity(float progress, bool restore_opacity);

  // Updates the y position of the apps container elements for the current app
  // list view position.
  // |progress| - The current app list view drag progress.
  void UpdateContentsYPosition(float progress);

  // Suggestion chips and apps grid view become unfocusable if |disabled| is
  // true. This is used to trap focus within the folder when it is opened.
  void DisableFocusForShowingActiveFolder(bool disabled);

  // Returns expected suggestion chip container's y position based on the app
  // list transition progress.
  int GetExpectedSuggestionChipY(float progress);

  struct GridLayout {
    int columns;
    int rows;
    int first_page_rows;
  };
  // Returns the number of columns and rows |apps_grid_view_| should display,
  // depending on the current display work area size.
  GridLayout CalculateGridLayout() const;

  // Calculates the grid layout and updates the number of rows and columns shown
  // in the top level apps grid.
  void UpdateTopLevelGridDimensions();

  // Returns the space available to the apps grid for laying out its contents.
  gfx::Rect CalculateAvailableBoundsForAppsGrid(
      const gfx::Rect& contents_bounds) const;

  // Depending on the provided apps container contents bounds and grid layout,
  // updates `app_list_config_` to be used within the apps container, and passes
  // it on to child views that require it.
  void UpdateAppListConfig(const gfx::Rect& contents_bounds);

  // Updates the apps container UI to display contents from the active app list
  // model. Should be called to initialize the apps container contents, and
  // whenever the active app list model changes.
  void UpdateForActiveAppListModel();

  // Callback returned by DisableBlur().
  void OnSuggestionChipsBlurDisablerReleased();

  // Updates the bounds of the gradient mask to fit the current bounds of the
  // `scrollable_container_`.
  void UpdateGradientMaskBounds();

  // Creates a layer mask for gradient alpha when the feature is enabled. The
  // gradient appears at the top and bottom of the 'scrollable_container_' to
  // create a "fade out" effect when dragging the whole page.
  void MaybeCreateGradientMask();

  // Removes the gradient mask from being set as the mask layer.
  void MaybeRemoveGradientMask();

  // Called when the animation to fade out app list items is completed.
  // `aborted` indicates whether the fade out animation is aborted.
  void OnAppsGridViewFadeOutAnimationEnded(
      const absl::optional<AppListSortOrder>& new_order,
      bool aborted);

  // Called when the animation to fade in app list items is completed.
  // `aborted` indicates whether the fade in animation is aborted.
  void OnAppsGridViewFadeInAnimationEnded(bool aborted);

  // Called at the end of the reorder animation. In detail, it is executed in
  // the following scenarios:
  // (1) At the end of the fade out animation when the fade out is aborted, or
  // (2) At the end of the fade in animation.
  void OnReorderAnimationEnded();

  // Called after sort to handle focus.
  void HandleFocusAfterSort();

  // While true, the gradient mask will not be removed as a mask layer until
  // cardified state ends.
  bool keep_gradient_mask_for_cardified_state_ = false;

  ContentsView* const contents_view_;

  // The app list config used to configure sizing and layout of apps grid items
  // within the apps container.
  std::unique_ptr<AppListConfig> app_list_config_;

  std::unique_ptr<AppListNudgeController> app_list_nudge_controller_;

  // Controller for showing a modal dialog in the continue section.
  std::unique_ptr<SearchResultPageDialogController> dialog_controller_;

  // The number of active requests to disable blur.
  size_t suggestion_chips_blur_disabler_count_ = 0;

  // Contains the |continue_section_| and the |apps_grid_view_|, which are views
  // that are affected by paging. Owned by views hierarchy.
  views::View* scrollable_container_ = nullptr;

  // The views below are owned by views hierarchy.
  SuggestionChipContainerView* suggestion_chip_container_view_ = nullptr;
  ContinueContainer* continue_container_ = nullptr;
  views::Separator* separator_ = nullptr;
  AppListToastContainerView* toast_container_ = nullptr;
  PagedAppsGridView* apps_grid_view_ = nullptr;
  AppListFolderView* app_list_folder_view_ = nullptr;
  PageSwitcher* page_switcher_ = nullptr;
  FolderBackgroundView* folder_background_view_ = nullptr;

  ShowState show_state_ = SHOW_NONE;

  // Whether the apps container is the current active app list page.
  bool is_active_page_ = false;

  // The distance between y position of suggestion chip container and apps grid
  // view. This is used in dragging to avoid duplicate calculation of apps grid
  // view's y position.
  int chip_grid_y_distance_ = 0;

  struct CachedContainerMargins {
    gfx::Size bounds_size;
    gfx::Size search_box_size;
    gfx::Insets margins;
  };
  // The last result returned by CalculateMarginsForAvailableBounds() -
  // subsequent calls to that method will return the result cached in
  // |cached_container_margins_|, provided the method arguments match the cached
  // arguments (otherwise the margins will be recalculated).
  CachedContainerMargins cached_container_margins_;

  std::unique_ptr<GradientLayerDelegate> gradient_layer_delegate_;

  // A closure to update item positions. It should run at the end of the fade
  // out animation when items are reordered.
  base::OnceClosure update_position_closure_;

  // A closure that runs at the end of the reorder animation.
  base::OnceClosure reorder_animation_done_closure_;

  base::WeakPtrFactory<AppsContainerView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_APPS_CONTAINER_VIEW_H_
