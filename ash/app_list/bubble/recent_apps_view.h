// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_BUBBLE_RECENT_APPS_VIEW_H_
#define ASH_APP_LIST_BUBBLE_RECENT_APPS_VIEW_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace ash {

class AppListItemView;
class AppListViewDelegate;

// The recent apps row in the "Continue" section of the bubble launcher. Shows
// a list of app icons.
class ASH_EXPORT RecentAppsView : public views::View {
 public:
  METADATA_HEADER(RecentAppsView);

  explicit RecentAppsView(AppListViewDelegate* view_delegate);
  RecentAppsView(const RecentAppsView&) = delete;
  RecentAppsView& operator=(const RecentAppsView&) = delete;
  ~RecentAppsView() override;

  AppListItemView* GetItemViewForTest(int index);

 private:
  AppListViewDelegate* const view_delegate_;

  // The grid delegate for each AppListItemView.
  class GridDelegateImpl;
  std::unique_ptr<GridDelegateImpl> grid_delegate_;
};

}  // namespace ash

#endif  // ASH_APP_LIST_BUBBLE_RECENT_APPS_VIEW_H_
