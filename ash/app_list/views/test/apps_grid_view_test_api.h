// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_TEST_APPS_GRID_VIEW_TEST_API_H_
#define ASH_APP_LIST_VIEWS_TEST_APPS_GRID_VIEW_TEST_API_H_

#include "ash/app_list/views/apps_grid_view.h"
#include "base/macros.h"

namespace gfx {
class Rect;
}

namespace views {
class View;
}

namespace ash {

class AppListItemView;
class AppsGridView;

namespace test {

class AppsGridViewTestApi {
 public:
  explicit AppsGridViewTestApi(AppsGridView* view);
  ~AppsGridViewTestApi();

  views::View* GetViewAtModelIndex(int index) const;

  void LayoutToIdealBounds();

  gfx::Rect GetItemTileRectOnCurrentPageAt(int row, int col) const;

  void PressItemAt(int index);

  bool HasPendingPageFlip() const;

  int TilesPerPage(int page) const;

  int AppsOnPage(int page) const;

  AppListItemView* GetViewAtIndex(GridIndex index) const;

  views::View* GetViewAtVisualIndex(int page, int slot) const;

  gfx::Rect GetItemTileRectAtVisualIndex(int page, int slot) const;

  void WaitForItemMoveAnimationDone();

 private:
  AppsGridView* view_;

  DISALLOW_COPY_AND_ASSIGN(AppsGridViewTestApi);
};

}  // namespace test
}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_TEST_APPS_GRID_VIEW_TEST_API_H_
