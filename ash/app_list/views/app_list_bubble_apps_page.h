// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_APP_LIST_BUBBLE_APPS_PAGE_H_
#define ASH_APP_LIST_VIEWS_APP_LIST_BUBBLE_APPS_PAGE_H_

#include "ash/app_list/views/apps_grid_view_focus_delegate.h"
#include "ash/app_list/views/recent_apps_view.h"
#include "ash/ash_export.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace ash {

class AppListConfig;
class ApplicationDragAndDropHost;
class AppListA11yAnnouncer;
class AppListFolderController;
class AppListViewDelegate;
class ContinueSectionView;
class RecentAppsView;
class ScrollableAppsGridView;

// The default page for the app list bubble / clamshell launcher. Contains a
// scroll view with:
// - Continue section with recent tasks and recent apps
// - Grid of all apps
// Does not include the search box, which is owned by a parent view.
class ASH_EXPORT AppListBubbleAppsPage : public views::View,
                                         public RecentAppsView::Delegate,
                                         public AppsGridViewFocusDelegate {
 public:
  METADATA_HEADER(AppListBubbleAppsPage);

  AppListBubbleAppsPage(AppListViewDelegate* view_delegate,
                        ApplicationDragAndDropHost* drag_and_drop_host,
                        AppListConfig* app_list_config,
                        AppListA11yAnnouncer* a11y_announcer,
                        AppListFolderController* folder_controller);
  AppListBubbleAppsPage(const AppListBubbleAppsPage&) = delete;
  AppListBubbleAppsPage& operator=(const AppListBubbleAppsPage&) = delete;
  ~AppListBubbleAppsPage() override;

  // Disables all children so they cannot be focused, allowing the open folder
  // view to handle focus.
  void DisableFocusForShowingActiveFolder(bool disabled);

  // RecentAppsView::Delegate:
  void MoveFocusUpFromRecents() override;
  void MoveFocusDownFromRecents(int column) override;

  // AppsGridViewFocusDelegate:
  bool MoveFocusUpFromAppsGrid(int column) override;

  views::ScrollView* scroll_view() { return scroll_view_; }
  ScrollableAppsGridView* scrollable_apps_grid_view() {
    return scrollable_apps_grid_view_;
  }

  RecentAppsView* recent_apps_for_test() { return recent_apps_; }

 private:
  friend class AppListTestHelper;

  ContinueSectionView* continue_section_ = nullptr;
  RecentAppsView* recent_apps_ = nullptr;
  views::ScrollView* scroll_view_ = nullptr;
  ScrollableAppsGridView* scrollable_apps_grid_view_ = nullptr;
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_APP_LIST_BUBBLE_APPS_PAGE_H_
