// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_RECENT_APPS_VIEW_H_
#define ASH_APP_LIST_VIEWS_RECENT_APPS_VIEW_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/app_list/model/app_list_model_observer.h"
#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace views {
class BoxLayout;
}

namespace ash {

class AppListKeyboardController;
class AppListModel;
class AppListConfig;
class AppListItemView;
class AppListViewDelegate;
class SearchModel;

// The recent apps row in the "Continue" section of the bubble launcher. Shows
// a list of app icons.
class ASH_EXPORT RecentAppsView : public AppListModelObserver,
                                  public views::View {
  METADATA_HEADER(RecentAppsView, views::View)

 public:
  RecentAppsView(AppListKeyboardController* keyboard_controller,
                 AppListViewDelegate* view_delegate);
  RecentAppsView(const RecentAppsView&) = delete;
  RecentAppsView& operator=(const RecentAppsView&) = delete;
  ~RecentAppsView() override;

  // AppListModelObserver:
  void OnAppListItemWillBeDeleted(AppListItem* item) override;

  // Sets the `AppListConfig` that should be used to configure layout of
  // `AppListItemViews` shown within this view.
  void UpdateAppListConfig(const AppListConfig* app_list_config);

  // Sets the search model and app list model used to obtain the list of the
  // most recent apps. Should be called at least once, otherwise the recent apps
  // view will not display any results.
  void SetModels(SearchModel* search_model, AppListModel* model);

  // Updates the visibility of this view. The view may be hidden if there are
  // not enough suggestions or if the user has chosen to hide it.
  void UpdateVisibility();

  // Returns the number of AppListItemView children.
  int GetItemViewCount() const;

  // Returns an AppListItemView child. `index` must be valid.
  AppListItemView* GetItemViewAt(int index) const;

  // See AppsGridView::DisableFocusForShowingActiveFolder().
  void DisableFocusForShowingActiveFolder(bool disabled);

  // views::View:
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;

 private:
  // Updates the recent apps view contents to show results provided by the
  // search model. Should be called at least once, otherwise the recent apps
  // view will not display any results.
  void UpdateResults(const std::vector<std::string>& ids_to_ignore);

  // Requests that focus move up and out (usually to the continue tasks).
  void MoveFocusUp();

  // Requests that focus move down and out (usually to the apps grid).
  void MoveFocusDown();

  // Returns the visual column of the child with focus, or -1 if there is none.
  int GetColumnOfFocusedChild() const;

  // Calculates how much padding is assigned to the AppListItemView.
  int CalculateTilePadding() const;

  const raw_ptr<AppListKeyboardController, DanglingUntriaged>
      keyboard_controller_;
  const raw_ptr<AppListViewDelegate> view_delegate_;
  raw_ptr<const AppListConfig, DanglingUntriaged> app_list_config_ = nullptr;
  raw_ptr<views::BoxLayout> layout_ = nullptr;
  raw_ptr<AppListModel> model_ = nullptr;
  raw_ptr<SearchModel> search_model_ = nullptr;

  // The grid delegate for each AppListItemView.
  class GridDelegateImpl;
  std::unique_ptr<GridDelegateImpl> grid_delegate_;

  // The recent app items. Stored here because this view has child views for
  // spacing that are not AppListItemViews.
  std::vector<raw_ptr<AppListItemView, VectorExperimental>> item_views_;
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_RECENT_APPS_VIEW_H_
