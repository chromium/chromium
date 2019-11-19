// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_APPS_CONTAINER_VIEW_H_
#define ASH_APP_LIST_VIEWS_APPS_CONTAINER_VIEW_H_

#include <stddef.h>

#include <vector>

#include "ash/app_list/model/app_list_folder_item.h"
#include "ash/app_list/views/horizontal_page.h"
#include "base/macros.h"

namespace ash {

class AppsGridView;
class ApplicationDragAndDropHost;
class AppListFolderItem;
class AppListFolderView;
class AppListModel;
class ContentsView;
class FolderBackgroundView;
class PageSwitcher;
class SuggestionChipContainerView;

// AppsContainerView contains a root level AppsGridView to render the root level
// app items, and a AppListFolderView to render the app items inside the
// active folder.
class APP_LIST_EXPORT AppsContainerView : public HorizontalPage {
 public:
  // Returns the minimum horizontal margins that apps grid has to respect.
  static int GetMinimumGridHorizontalMargin();

  AppsContainerView(ContentsView* contents_view, AppListModel* model);
  ~AppsContainerView() override;

  // Shows the active folder content specified by |folder_item|.
  void ShowActiveFolder(AppListFolderItem* folder_item);

  // Shows the root level apps list. This is called when UI navigate back from
  // a folder view with |folder_item|. If |folder_item| is nullptr skips
  // animation.
  void ShowApps(AppListFolderItem* folder_item);

  // Resets the app list to a state where it shows the main grid view. This is
  // called when the user opens the launcher for the first time or when the user
  // hides and then shows it. This is necessary because we only hide and show
  // the launcher on Windows and Linux so we need to reset to a fresh state.
  void ResetForShowApps();

  // Sets |drag_and_drop_host_| for the current app list in both
  // app_list_folder_view_ and root level apps_grid_view_.
  void SetDragAndDropHostOfCurrentAppList(
      ApplicationDragAndDropHost* drag_and_drop_host);

  // Transits the UI from folder view to root level apps grid view when
  // re-parenting a child item of |folder_item|.
  void ReparentFolderItemTransit(AppListFolderItem* folder_item);

  // Returns true if it is currently showing an active folder page.
  bool IsInFolderView() const;

  // Called to notify the AppsContainerView that a reparent drag has completed.
  void ReparentDragEnded();

  // Updates the visibility of the items in this view according to
  // |app_list_state| and |is_in_drag|.
  void UpdateControlVisibility(ash::AppListViewState app_list_state,
                               bool is_in_drag);

  // Animates the container opacity from the opacity for the current app list
  // transition progress to the opacity for |target_view_state|.
  // |current_progress| - the current app list transition progress.
  // |animator| - callback that when run starts the opacity animation.
  using OpacityAnimator =
      base::RepeatingCallback<void(views::View*, bool target_visibility)>;
  void AnimateOpacity(float current_progress,
                      ash::AppListViewState target_view_state,
                      const OpacityAnimator& animator);

  // Sets the expected y position for apps container children, and runs
  // |animator| for each of them to run transform animation from current bounds.
  // (This assumes that animator knows the offset between current apps container
  // bounds and target apps container bounds).
  using TransformAnimator = base::RepeatingCallback<void(views::View*)>;
  void AnimateYPosition(ash::AppListViewState target_view_state,
                        const TransformAnimator& animator);

  // Updates y position and opacity of the items in this view during dragging.
  void UpdateYPositionAndOpacity(float progress, bool restore_opacity);

  // Called when tablet mode starts and ends.
  void OnTabletModeChanged(bool started);

  // Calculates the apps container or apps grid margin depending on the
  // available content bounds, and search box size (the later is not used
  // if |for_full_container_bounds| is false).
  // |available_bounds| - The bounds available to lay out either full apps
  //      container or apps grid (depending on |for_full_contaier_bounds|).
  // |search_box_size| - The expected search box size. Used to determine the
  //      the amount of space in apps container available to the apps grid
  //      (if calaulating margins for apps grid, |available_bounds| should
  //      not contain the search box, so this value will not be used in that
  //      case).
  // |for_full_container_bounds| - Whether the bounds are being calculated
  //      for the whole apps container, or just the apps grid. It should be true
  //      iff app_list_features::IsScalableAppListEnabled().
  //
  // NOTE: This should not call into ContentsView::GetSearchBoxBounds*()
  // methods, as CalculateMarginsForAvailableBounds is used to calculate the
  // search box bounds.
  const gfx::Insets& CalculateMarginsForAvailableBounds(
      const gfx::Rect& available_bounds,
      const gfx::Size& search_box_size,
      bool for_full_container_bounds);

  // views::View overrides:
  void Layout() override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  const char* GetClassName() const override;
  void OnGestureEvent(ui::GestureEvent* event) override;

  // HorizontalPage overrides:
  void OnWillBeHidden() override;
  views::View* GetFirstFocusableView() override;
  gfx::Rect GetPageBoundsForState(ash::AppListState state) const override;

  SuggestionChipContainerView* suggestion_chip_container_view_for_test() {
    return suggestion_chip_container_view_;
  }
  AppsGridView* apps_grid_view() { return apps_grid_view_; }
  FolderBackgroundView* folder_background_view() {
    return folder_background_view_;
  }
  AppListFolderView* app_list_folder_view() { return app_list_folder_view_; }
  PageSwitcher* page_switcher() { return page_switcher_; }

  // Updates suggestion chips from app list model.
  void UpdateSuggestionChips();

 private:
  enum ShowState {
    SHOW_NONE,  // initial state
    SHOW_APPS,
    SHOW_ACTIVE_FOLDER,
    SHOW_ITEM_REPARENT,
  };

  // Returns the AppListConfig for the app list view this AppsContainerView
  // belongs to.
  const AppListConfig& GetAppListConfig() const;

  void SetShowState(ShowState show_state, bool show_apps_with_animation);

  // Suggestion chips and apps grid view become unfocusable if |disabled| is
  // true. This is used to trap focus within the folder when it is opened.
  void DisableFocusForShowingActiveFolder(bool disabled);

  // Gets the suggestion chips container top margin for the app list transition
  // progress.
  int GetSuggestionChipContainerTopMargin(float progress) const;

  // Returns expected suggestion chip container's y position based on the app
  // list transition progress.
  int GetExpectedSuggestionChipY(float progress);

  struct GridLayout {
    int columns;
    int rows;
  };
  // Returns the number of columns and rows |apps_grid_view_| should display,
  // depending on the current display work area size.
  GridLayout CalculateGridLayout() const;

  ContentsView* contents_view_;  // Not owned.

  // The views below are owned by views hierarchy.
  SuggestionChipContainerView* suggestion_chip_container_view_ = nullptr;
  AppsGridView* apps_grid_view_ = nullptr;
  AppListFolderView* app_list_folder_view_ = nullptr;
  PageSwitcher* page_switcher_ = nullptr;
  FolderBackgroundView* folder_background_view_ = nullptr;

  ShowState show_state_ = SHOW_NONE;

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

  DISALLOW_COPY_AND_ASSIGN(AppsContainerView);
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_APPS_CONTAINER_VIEW_H_
