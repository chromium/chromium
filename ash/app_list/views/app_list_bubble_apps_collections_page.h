// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_APP_LIST_BUBBLE_APPS_COLLECTIONS_PAGE_H_
#define ASH_APP_LIST_VIEWS_APP_LIST_BUBBLE_APPS_COLLECTIONS_PAGE_H_

#include <memory>
#include <string>

#include "ash/app_list/app_list_model_provider.h"
#include "ash/app_list/app_list_view_provider.h"
#include "ash/app_list/views/app_list_item_view.h"
#include "ash/app_list/views/app_list_item_view_grid_delegate.h"
#include "ash/app_list/views/app_list_nudge_controller.h"
#include "ash/app_list/views/app_list_toast_container_view.h"
#include "ash/ash_export.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/views/view.h"

namespace views {
class ScrollView;
}

namespace ash {
class RoundedScrollBar;
class AppListConfig;
class AppListNudgeController;
class AppsGridContextMenu;
class SearchResultPageDialogController;

// A page for the bubble / clamshell launcher. Contains a scroll view with
// subsections of apps, one per each category of the Apps Collections. It also
// contains an informational nudge that dismisses the page when acknowledged.
// Does not include the search box, which is owned by a parent view.
class ASH_EXPORT AppListBubbleAppsCollectionsPage
    : public AppListItemViewGridDelegate,
      public AppListToastContainerView::Delegate,
      public AppListModelProvider::Observer,
      public views::View {
  METADATA_HEADER(AppListBubbleAppsCollectionsPage, views::View)

 public:
  AppListBubbleAppsCollectionsPage(
      AppListViewDelegate* view_delegate,
      AppListConfig* app_list_config,
      AppListA11yAnnouncer* a11y_announcer,
      SearchResultPageDialogController* dialog_controller,
      base::OnceClosure exit_page_callback);
  AppListBubbleAppsCollectionsPage(const AppListBubbleAppsCollectionsPage&) =
      delete;
  AppListBubbleAppsCollectionsPage& operator=(
      const AppListBubbleAppsCollectionsPage&) = delete;
  ~AppListBubbleAppsCollectionsPage() override;

  // Invoked when a user clicks on the `disocovery_chip_`.
  void OnDiscoveryChipPressed();

  // Starts the animation for showing the page, coming from another page.
  void AnimateShowPage();

  // Starts the animation for hiding the page, going to another page.
  void AnimateHidePage();

  // Aborts all layer animations, which invokes their cleanup callbacks.
  void AbortAllAnimations();

  // AppListToastContainerView::Delegate:
  void OnNudgeRemoved() override;
  AppsGridContextMenu::GridType GetGridTypeForContextMenu() override;

  // AppListModelProvider::Observer:
  void OnActiveAppListModelsChanged(AppListModel* model,
                                    SearchModel* search_model) override;
  // AppListItemView::GridDelegate:
  bool IsInFolder() const override;
  void SetSelectedView(AppListItemView* view) override;
  void ClearSelectedView() override;
  bool IsSelectedView(const AppListItemView* view) const override;
  void EndDrag(bool cancel) override;
  void OnAppListItemViewActivated(AppListItemView* pressed_item_view,
                                  const ui::Event& event) override;
  bool IsAboveTheFold(AppListItemView* item_view) override;

  // Updates the controller that the page uses to show dialogs.
  void SetDialogController(SearchResultPageDialogController* dialog_controller);

  void RecordAboveTheFoldMetrics();

  // Which layer animates is an implementation detail.
  ui::Layer* GetPageAnimationLayerForTest();

  // The view that contains the informational nudge for this page.
  AppListToastContainerView* GetToastContainerViewForTest();

  views::ScrollView* scroll_view() { return scroll_view_; }

  AppsGridContextMenu* context_menu_for_test() { return context_menu_.get(); }

  views::View* discovery_chip_for_test() { return discovery_chip_; }

 private:
  friend class AppListTestHelper;

  // A callback invoked to update the visibility of the page contents after an
  // animation is done.
  void SetVisibilityAfterAnimation(bool visible);

  void PopulateCollections(AppListModel* model);

  // Invoked when the user attempts to sort apps from the AppsCollection page.
  void RequestAppReorder(AppListSortOrder order);

  // Invoked when the user causes the dismissal of the AppsCollections page by
  // reordering the apps.
  void DismissPageAndReorder(AppListSortOrder order);

  // Invoked when the `scroll_view_` received an scrolling event.
  void OnPageScrolled();

  const raw_ptr<AppListViewDelegate> view_delegate_;
  raw_ptr<views::ScrollView> scroll_view_ = nullptr;
  raw_ptr<RoundedScrollBar> scroll_bar_ = nullptr;
  raw_ptr<AppListToastContainerView> toast_container_ = nullptr;
  raw_ptr<views::View> sections_container_ = nullptr;
  const raw_ptr<AppListConfig> app_list_config_;
  raw_ptr<views::View> discovery_chip_ = nullptr;

  raw_ptr<SearchResultPageDialogController> dialog_controller_ = nullptr;

  std::unique_ptr<AppListNudgeController> app_list_nudge_controller_;

  std::unique_ptr<AppsGridContextMenu> context_menu_;

  // A callback invoked when the nudge on this page is removed/dismissed.
  base::OnceClosure exit_page_callback_;

  // Subscription to notify of scrolling events.
  base::CallbackListSubscription on_contents_scrolled_subscription_;

  // The last observed offset between the scroll view's visible and contents
  // bounds bottoms.
  std::optional<int> last_bottom_scroll_offset_;

  // The currently selected view.
  raw_ptr<AppListItemView> selected_view_ = nullptr;

  base::WeakPtrFactory<AppListBubbleAppsCollectionsPage> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_APP_LIST_BUBBLE_APPS_COLLECTIONS_PAGE_H_
