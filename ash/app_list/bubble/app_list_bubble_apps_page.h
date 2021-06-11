// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_BUBBLE_APP_LIST_BUBBLE_APPS_PAGE_H_
#define ASH_APP_LIST_BUBBLE_APP_LIST_BUBBLE_APPS_PAGE_H_

#include "ash/app_list/bubble/scrollable_apps_grid_view.h"
#include "ash/ash_export.h"
#include "ui/views/view.h"

namespace ash {

class AppListViewDelegate;
class RecentAppsView;
class ScrollableAppsGridView;

// The default page for the app list bubble / clamshell launcher. Contains a
// scroll view with:
// - Continue section with recent tasks and recent apps
// - Grid of all apps
// Does not include the search box, which is owned by a parent view.
class ASH_EXPORT AppListBubbleAppsPage : public views::View {
 public:
  explicit AppListBubbleAppsPage(AppListViewDelegate* view_delegate);
  AppListBubbleAppsPage(const AppListBubbleAppsPage&) = delete;
  AppListBubbleAppsPage& operator=(const AppListBubbleAppsPage&) = delete;
  ~AppListBubbleAppsPage() override;

  RecentAppsView* recent_apps_for_test() { return recent_apps_; }
  ScrollableAppsGridView* scrollable_apps_grid_view_for_test() {
    return scrollable_apps_grid_view_;
  }

 private:
  RecentAppsView* recent_apps_ = nullptr;
  ScrollableAppsGridView* scrollable_apps_grid_view_ = nullptr;
};

}  // namespace ash

#endif  // ASH_APP_LIST_BUBBLE_APP_LIST_BUBBLE_APPS_PAGE_H_
