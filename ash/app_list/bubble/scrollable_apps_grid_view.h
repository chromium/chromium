// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_BUBBLE_SCROLLABLE_APPS_GRID_VIEW_H_
#define ASH_APP_LIST_BUBBLE_SCROLLABLE_APPS_GRID_VIEW_H_

#include "ash/app_list/views/apps_grid_view.h"
#include "ash/ash_export.h"

namespace ash {

class AppListViewDelegate;

// An apps grid that shows all the apps in a long scrolling list. Used for
// the clamshell mode bubble launcher.
class ASH_EXPORT ScrollableAppsGridView : public AppsGridView {
 public:
  ScrollableAppsGridView(AppListViewDelegate* view_delegate,
                         AppsGridViewFolderDelegate* folder_delegate);
  ScrollableAppsGridView(const ScrollableAppsGridView&) = delete;
  ScrollableAppsGridView& operator=(const ScrollableAppsGridView&) = delete;
  ~ScrollableAppsGridView() override;

  // views::View:
  void Layout() override;

  // AppsGridView:
  gfx::Insets GetTilePadding() const override;
  gfx::Size GetTileGridSize() const override;
  void CalculateIdealBounds() override;
};

}  // namespace ash

#endif  // ASH_APP_LIST_BUBBLE_SCROLLABLE_APPS_GRID_VIEW_H_
