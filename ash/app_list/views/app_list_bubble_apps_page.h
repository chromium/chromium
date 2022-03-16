// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_APP_LIST_BUBBLE_APPS_PAGE_H_
#define ASH_APP_LIST_VIEWS_APP_LIST_BUBBLE_APPS_PAGE_H_

#include <memory>

#include "ash/app_list/app_list_model_provider.h"
#include "ash/app_list/views/app_list_nudge_controller.h"
#include "ash/app_list/views/apps_grid_view_focus_delegate.h"
#include "ash/app_list/views/recent_apps_view.h"
#include "ash/ash_export.h"
#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"

namespace base {
class TimeDelta;
}  // namespace base

namespace ui {
class Layer;
}  // namespace ui

namespace views {
class Separator;
}  // namespace views

namespace ash {

class AppListConfig;
class ApplicationDragAndDropHost;
class AppListA11yAnnouncer;
class AppListFolderController;
class AppListNudgeController;
class AppListToastContainerView;
class AppListViewDelegate;
class ContinueSectionView;
class RecentAppsView;
class ScrollableAppsGridView;
class ScrollViewGradientHelper;

// The default page for the app list bubble / clamshell launcher. Contains a
// scroll view with:
// - Continue section with recent tasks and recent apps
// - Grid of all apps
// Does not include the search box, which is owned by a parent view.
class ASH_EXPORT AppListBubbleAppsPage : public views::View,
                                         public views::ViewObserver,
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

  // Updates the continue section and recent apps.
  void UpdateSuggestions();

  // Starts the launcher show animation.
  void AnimateShowLauncher(bool is_side_shelf);

  // Starts the launcher hide animation. None of the child views animate, but
  // this disables the scroll view gradient mask to improve performance.
  void AnimateHideLauncher();

  // Starts the animation for showing the apps page, coming from another page.
  void AnimateShowPage();

  // Starts the animation for hiding the apps page, going to another page.
  void AnimateHidePage();

  // Resets the scroll position to the top.
  void ResetScrollPosition();

  // Aborts all layer animations, which invokes their cleanup callbacks.
  void AbortAllAnimations();

  // Disables all children so they cannot be focused, allowing the open folder
  // view to handle focus.
  void DisableFocusForShowingActiveFolder(bool disabled);

  // Handles `AppListController::UpdateAppListWithNewSortingOrder()` for the
  // bubble launcher apps page.
  void UpdateForNewSortingOrder(
      const absl::optional<AppListSortOrder>& new_order,
      bool animate,
      base::OnceClosure update_position_closure);

  // Scrolls to fully show the toast if the toast is partially shown or hidden
  // from the scroll view's perspective. Returns true if scrolling is performed.
  bool MaybeScrollToShowToast();

  // views::View:
  void Layout() override;
  void VisibilityChanged(views::View* starting_from, bool is_visible) override;

  // view::ViewObserver:
  void OnViewVisibilityChanged(views::View* observed_view,
                               views::View* starting_view) override;

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

  // Which layer animates is an implementation detail.
  ui::Layer* GetPageAnimationLayerForTest();

  RecentAppsView* recent_apps_for_test() { return recent_apps_; }
  views::Separator* separator_for_test() { return separator_; }
  AppListToastContainerView* toast_container_for_test() {
    return toast_container_;
  }

  AppListNudgeController* app_list_nudge_controller() {
    return app_list_nudge_controller_.get();
  }

  ScrollViewGradientHelper* gradient_helper_for_test() {
    return gradient_helper_.get();
  }

 private:
  friend class AppListTestHelper;

  void UpdateSeparatorVisibility();

  // Destroys the layer for `view`. Not static so it can be used with weak
  // pointers.
  void DestroyLayerForView(views::View* view);

  // Callback for when the apps grid view animation ends.
  void OnAppsGridViewAnimationEnded();

  // Called when the animation to fade out app list items is completed.
  // `aborted` indicates whether the fade out animation is aborted.
  void OnAppsGridViewFadeOutAnimationEneded(
      const absl::optional<AppListSortOrder>& new_order,
      bool aborted);

  // Called when the animation to fade in app list items is completed.
  // `aborted` indicates whether the fade in animation is aborted.
  void OnAppsGridViewFadeInAnimationEnded(bool aborted);

  // Called at the end of the reorder animation. In detail, it is executed in
  // the following scenarios:
  // (1) At the end of the fade out animation when the fade out is aborted, or
  // (2) At the end of the fade in animation.
  void OnReorderAnimationEnded();

  // Animates `view` using a layer animation. Creates the layer if needed. The
  // layer is pushed down by `vertical_offset` at the start of the animation and
  // animates back to its original position with `duration`.
  void SlideViewIntoPosition(views::View* view,
                             int vertical_offset,
                             base::TimeDelta duration);

  views::ScrollView* scroll_view_ = nullptr;
  ContinueSectionView* continue_section_ = nullptr;
  RecentAppsView* recent_apps_ = nullptr;
  views::Separator* separator_ = nullptr;
  AppListToastContainerView* toast_container_ = nullptr;
  ScrollableAppsGridView* scrollable_apps_grid_view_ = nullptr;

  std::unique_ptr<AppListNudgeController> app_list_nudge_controller_;

  // Adds fade in/out gradients to `scroll_view_`.
  std::unique_ptr<ScrollViewGradientHelper> gradient_helper_;

  // A closure to update item positions. It should run at the end of the fade
  // out animation when items are reordered.
  base::OnceClosure update_position_closure_;

  base::WeakPtrFactory<AppListBubbleAppsPage> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_APP_LIST_BUBBLE_APPS_PAGE_H_
