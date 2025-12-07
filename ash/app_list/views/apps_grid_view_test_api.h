// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_APPS_GRID_VIEW_TEST_API_H_
#define ASH_APP_LIST_VIEWS_APPS_GRID_VIEW_TEST_API_H_

#include <string>

#include "ash/app_list/views/apps_grid_view.h"
#include "base/memory/raw_ptr.h"

namespace gfx {
class Rect;
}

namespace views {
class View;
}

namespace ash {

class AppListItemView;

namespace test {

class AppsGridViewTestApi {
 public:
  explicit AppsGridViewTestApi(AppsGridView* view);

  AppsGridViewTestApi(const AppsGridViewTestApi&) = delete;
  AppsGridViewTestApi& operator=(const AppsGridViewTestApi&) = delete;

  ~AppsGridViewTestApi();

  views::View* GetViewAtModelIndex(int index) const;

  void LayoutToIdealBounds();

  // Returns tile bounds for item in the provided `row` and `col` on the current
  // apps grid page. It does not require an item to exist in the provided spot.
  // NOTE: In RTL layout column with index 0 will be rightmost column.
  gfx::Rect GetItemTileRectOnCurrentPageAt(int row, int col) const;

  void PressItemAt(int index);

  // Returns the number of tiles per page in paged apps grid. It should not be
  // called for scrollable apps grid, in which case number of tiles per page is
  // not defined.
  size_t TilesPerPageInPagedGrid(int page) const;

  // Returns number of tiles allowed on the page for paged apps grid, or
  // `default_value` for scrollable apps grid, for which number of tiles per
  // page is not defined.
  size_t TilesPerPageOr(int page, size_t default_value) const;

  int AppsOnPage(int page) const;

  AppListItemView* GetViewAtIndex(GridIndex index) const;

  AppListItemView* GetViewAtVisualIndex(int page, int slot) const;

  // Returns the name of the item specified by the grid location.
  const std::string& GetNameAtVisualIndex(int page, int slot) const;

  // Returns tile bounds for item in the provided grid `slot` and `page`.
  // Item slot indicates the index of the item in the apps grid.
  // NOTE: In RTL UI, slot 0 is the top right position in the grid.
  gfx::Rect GetItemTileRectAtVisualIndex(int page, int slot) const;

  void WaitForItemMoveAnimationDone();

  // Fires the reordering timer if the timer is running. Then waits for the
  // reordering animation to complete.
  void FireReorderTimerAndWaitForAnimationDone();

  void Update() { view_->Update(); }

  // Moves the app list item at `source_index` to `target_index` by drag and
  // drop. `source_index` and `target_index` are view indices in `view_`.
  void ReorderItemByDragAndDrop(int source_index, int target_index);

  AppListItemList* GetItemList() { return view_->item_list_; }

 private:
  raw_ptr<AppsGridView, DanglingUntriaged> view_;
};

}  // namespace test
}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_APPS_GRID_VIEW_TEST_API_H_
