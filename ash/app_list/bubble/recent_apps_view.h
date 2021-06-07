// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_BUBBLE_RECENT_APPS_VIEW_H_
#define ASH_APP_LIST_BUBBLE_RECENT_APPS_VIEW_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "ui/views/view.h"

namespace ui {
class Event;
}  // namespace ui

namespace ash {

class AppListItem;
class AppListViewDelegate;

// The recent apps row in the "Continue" section of the bubble launcher. Shows
// a list of app icons.
class ASH_EXPORT RecentAppsView : public views::View {
 public:
  explicit RecentAppsView(AppListViewDelegate* view_delegate);
  RecentAppsView(const RecentAppsView&) = delete;
  RecentAppsView& operator=(const RecentAppsView&) = delete;
  ~RecentAppsView() override;

 private:
  // Adds an app icon as a child view.
  void AddAppIcon(AppListItem* item);

  // Callback for clicking on an app.
  void OnAppListItemViewPressed(const std::string& item_id,
                                const ui::Event& event);

  AppListViewDelegate* const view_delegate_;

  // The grid delegate for each AppListItemView.
  class GridDelegateImpl;
  std::unique_ptr<GridDelegateImpl> grid_delegate_;
};

}  // namespace ash

#endif  // ASH_APP_LIST_BUBBLE_RECENT_APPS_VIEW_H_
