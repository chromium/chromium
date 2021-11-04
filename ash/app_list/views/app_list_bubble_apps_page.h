// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_APP_LIST_BUBBLE_APPS_PAGE_H_
#define ASH_APP_LIST_VIEWS_APP_LIST_BUBBLE_APPS_PAGE_H_

#include <memory>

#include "ash/app_list/app_list_model_provider.h"
#include "ash/app_list/views/apps_grid_view_focus_delegate.h"
#include "ash/app_list/views/recent_apps_view.h"
#include "ash/ash_export.h"
#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace views {
class Separator;
}  // namespace views

namespace ash {

class AppListConfig;
class ApplicationDragAndDropHost;
class AppListA11yAnnouncer;
class AppListFolderController;
class AppListViewDelegate;
class ContinueSectionView;
class RecentAppsView;
class ScopedScrollViewGradientDisabler;
class ScrollableAppsGridView;
class ScrollViewGradientHelper;

// The default page for the app list bubble / clamshell launcher. Contains a
// scroll view with:
// - Continue section with recent tasks and recent apps
// - Grid of all apps
// Does not include the search box, which is owned by a parent view.
class ASH_EXPORT AppListBubbleAppsPage : public views::View,
                                         public AppListModelProvider::Observer,
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

  // Starts the launcher show animation.
  void StartShowAnimation();

  // Animates `view` using a layer animation. Creates the layer if needed. The
  // layer is pushed down by `vertical_offset` at the start of the animation and
  // animates back to its original position. Public for testing.
  void SlideViewIntoPosition(views::View* view, int vertical_offset);

  // views::View:
  void Layout() override;
  void ChildVisibilityChanged(views::View* child) override;

  // AppListModelProvider::Observer:
  void OnActiveAppListModelsChanged(AppListModel* model,
                                    SearchModel* search_model) override;

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
  views::Separator* separator_for_test() { return separator_; }

 private:
  friend class AppListTestHelper;

  void UpdateSeparatorVisibility();

  // Starts a vertical slide animation for `view` with `vertical_offset` as the
  // initial offset. The view must already have a layer. Runs the `cleanup`
  // callback when the animation ends or aborts.
  void StartSlideInAnimation(views::View* view,
                             int vertical_offset,
                             base::RepeatingClosure cleanup);

  // Destroys the layer for `view`. Not static so it can be used with weak
  // pointers.
  void DestroyLayerForView(views::View* view);

  // Callback for when the apps grid view animation ends.
  void OnAppsGridViewAnimationEnded();

  views::ScrollView* scroll_view_ = nullptr;
  ContinueSectionView* continue_section_ = nullptr;
  RecentAppsView* recent_apps_ = nullptr;
  views::Separator* separator_ = nullptr;
  ScrollableAppsGridView* scrollable_apps_grid_view_ = nullptr;

  // Adds fade in/out gradients to `scroll_view_`.
  std::unique_ptr<ScrollViewGradientHelper> gradient_helper_;

  // Disables the gradient on `scroll_view_` during animations to improve
  // performance. Must be destroyed before `gradient_helper_`.
  std::unique_ptr<ScopedScrollViewGradientDisabler> gradient_disabler_;

  base::WeakPtrFactory<AppListBubbleAppsPage> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_APP_LIST_BUBBLE_APPS_PAGE_H_
