// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/test/app_list_test_api.h"

#include <string>
#include <utility>
#include <vector>

#include "ash/app_list/app_list_bubble_presenter.h"
#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/app_list_model_provider.h"
#include "ash/app_list/app_list_presenter_impl.h"
#include "ash/app_list/app_list_public_test_util.h"
#include "ash/app_list/model/app_list_folder_item.h"
#include "ash/app_list/model/app_list_item.h"
#include "ash/app_list/model/app_list_model.h"
#include "ash/app_list/views/app_list_bubble_apps_page.h"
#include "ash/app_list/views/app_list_bubble_search_page.h"
#include "ash/app_list/views/app_list_bubble_view.h"
#include "ash/app_list/views/app_list_folder_view.h"
#include "ash/app_list/views/app_list_item_view.h"
#include "ash/app_list/views/app_list_main_view.h"
#include "ash/app_list/views/app_list_menu_model_adapter.h"
#include "ash/app_list/views/app_list_search_view.h"
#include "ash/app_list/views/app_list_toast_container_view.h"
#include "ash/app_list/views/app_list_toast_view.h"
#include "ash/app_list/views/app_list_view.h"
#include "ash/app_list/views/apps_container_view.h"
#include "ash/app_list/views/apps_grid_context_menu.h"
#include "ash/app_list/views/apps_grid_view.h"
#include "ash/app_list/views/apps_grid_view_test_api.h"
#include "ash/app_list/views/contents_view.h"
#include "ash/app_list/views/continue_section_view.h"
#include "ash/app_list/views/paged_apps_grid_view.h"
#include "ash/app_list/views/recent_apps_view.h"
#include "ash/app_list/views/scrollable_apps_grid_view.h"
#include "ash/app_list/views/search_box_view.h"
#include "ash/app_list/views/search_result_list_view.h"
#include "ash/app_list/views/search_result_page_view.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/accelerators.h"
#include "ash/shell.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window_observer.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/test/layer_animation_stopped_waiter.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/view_model.h"
#include "ui/views/view_utils.h"

namespace ash {

namespace {

// A global pointer to the disabler's instance. Used to ensure at most one
// disabler exists at a time.
class ScopedItemMoveAnimationDisabler;
ScopedItemMoveAnimationDisabler* g_disabler_ptr = nullptr;

// Returns the menu item indicated by `order` from a non-folder item menu.
views::MenuItemView* GetReorderOptionForNonFolderItemMenu(
    const views::MenuItemView* root_menu,
    ash::AppListSortOrder order) {
  // Get the last menu item index where the reorder submenu is.
  views::MenuItemView* reorder_item_view =
      root_menu->GetSubmenu()->GetLastItem();
  DCHECK_EQ(reorder_item_view->title(), u"Sort by");
  return reorder_item_view;
}

ash::AppListItemView* FindFolderItemView(ash::AppsGridView* apps_grid_view) {
  auto* model = apps_grid_view->view_model();
  for (size_t index = 0; index < model->view_size(); ++index) {
    ash::AppListItemView* current_view = model->view_at(index);
    if (current_view->is_folder())
      return current_view;
  }

  return nullptr;
}

ash::AppListItemView* FindNonFolderItemView(ash::AppsGridView* apps_grid_view) {
  auto* model = apps_grid_view->view_model();
  for (size_t index = 0; index < model->view_size(); ++index) {
    ash::AppListItemView* current_view = model->view_at(index);
    if (!current_view->is_folder())
      return current_view;
  }

  return nullptr;
}

// Returns the index of the specified sorting option.
size_t GetMenuIndexOfSortingOrder(ash::AppListSortOrder order) {
  switch (order) {
    case ash::AppListSortOrder::kNameAlphabetical:
      return 0;
    case ash::AppListSortOrder::kColor:
      return 1;
    case ash::AppListSortOrder::kNameReverseAlphabetical:
    case ash::AppListSortOrder::kCustom:
    case ash::AppListSortOrder::kAlphabeticalEphemeralAppFirst:
      NOTREACHED();
  }
}

views::MenuItemView* GetReorderOptionForAppListOrFolderItemMenu(
    const views::MenuItemView* root_menu,
    const ash::AppListSortOrder order) {
  views::MenuItemView* reorder_option = nullptr;
  switch (order) {
    case ash::AppListSortOrder::kNameAlphabetical:
      reorder_option = root_menu->GetSubmenu()->GetMenuItemAt(1);
      EXPECT_TRUE(reorder_option->title() == u"Name");
      break;
    case ash::AppListSortOrder::kColor:
      reorder_option = root_menu->GetSubmenu()->GetMenuItemAt(2);
      EXPECT_TRUE(reorder_option->title() == u"Color");
      break;
    case ash::AppListSortOrder::kNameReverseAlphabetical:
    case ash::AppListSortOrder::kCustom:
    case ash::AppListSortOrder::kAlphabeticalEphemeralAppFirst:
      NOTREACHED();
  }
  return reorder_option;
}

views::MenuItemView* ShowRootMenuAndReturn(
    ash::AppsGridView* apps_grid_view,
    AppListTestApi::MenuType menu_type,
    ui::test::EventGenerator* event_generator) {
  views::MenuItemView* root_menu = nullptr;

  EXPECT_GT(apps_grid_view->view_model()->view_size(), 0u);

  switch (menu_type) {
    case AppListTestApi::MenuType::kAppListPageMenu:
      event_generator->MoveMouseTo(
          apps_grid_view->GetBoundsInScreen().CenterPoint());
      event_generator->ClickRightButton();
      root_menu =
          apps_grid_view->context_menu_for_test()->root_menu_item_view();
      break;
    case AppListTestApi::MenuType::kAppListNonFolderItemMenu:
    case AppListTestApi::MenuType::kAppListFolderItemMenu:
      const bool is_folder_item =
          (menu_type == AppListTestApi::MenuType::kAppListFolderItemMenu);
      ash::AppListItemView* item_view =
          is_folder_item ? FindFolderItemView(apps_grid_view)
                         : FindNonFolderItemView(apps_grid_view);
      EXPECT_TRUE(item_view);
      event_generator->MoveMouseTo(
          item_view->GetBoundsInScreen().CenterPoint());
      event_generator->ClickRightButton();

      if (is_folder_item) {
        root_menu = item_view->context_menu_for_folder()->root_menu_item_view();
      } else {
        ash::AppListMenuModelAdapter* menu_model_adapter =
            item_view->item_menu_model_adapter();
        root_menu = menu_model_adapter->root_for_testing();
      }
      break;
  }

  EXPECT_TRUE(root_menu->SubmenuIsShowing());
  return root_menu;
}

PagedAppsGridView* GetPagedAppsGridView() {
  // This view only exists for tablet launcher and legacy peeking launcher.
  DCHECK(!ShouldUseBubbleAppList());
  return AppListView::TestApi(GetAppListView()).GetRootAppsGridView();
}

AppsContainerView* GetAppsContainerView() {
  return GetAppListView()
      ->app_list_main_view()
      ->contents_view()
      ->apps_container_view();
}

AppListFolderView* GetAppListFolderView() {
  // Handle the case that the app list bubble view is effective.
  if (ShouldUseBubbleAppList())
    return GetAppListBubbleView()->folder_view_for_test();

  return GetAppsContainerView()->app_list_folder_view();
}

AppListToastContainerView* GetToastContainerViewFromBubble() {
  return GetAppListBubbleView()
      ->apps_page_for_test()
      ->toast_container_for_test();
}

AppListToastContainerView* GetToastContainerViewFromFullscreenAppList() {
  return GetAppsContainerView()->toast_container();
}

RecentAppsView* GetRecentAppsView() {
  if (ShouldUseBubbleAppList())
    return GetAppListBubbleView()->apps_page_for_test()->recent_apps_for_test();

  return GetAppsContainerView()->GetRecentAppsView();
}

ContinueSectionView* GetContinueSectionView() {
  if (ShouldUseBubbleAppList()) {
    return GetAppListBubbleView()
        ->apps_page_for_test()
        ->GetContinueSectionView();
  }
  return GetAppsContainerView()->GetContinueSectionView();
}

AppListSearchView* GetSearchView() {
  if (ShouldUseBubbleAppList()) {
    return GetAppListBubbleView()->search_page()->search_view();
  }

  return GetAppListView()
      ->app_list_main_view()
      ->contents_view()
      ->search_result_page_view()
      ->search_view();
}

// AppListVisibilityChangedWaiter ----------------------------------------------

// Waits until the app list visibility changes.
class AppListVisibilityChangedWaiter : public AppListControllerObserver {
 public:
  AppListVisibilityChangedWaiter() = default;
  AppListVisibilityChangedWaiter(const AppListVisibilityChangedWaiter&) =
      delete;
  AppListVisibilityChangedWaiter& operator=(
      const AppListVisibilityChangedWaiter&) = delete;
  ~AppListVisibilityChangedWaiter() override {
    AppListController::Get()->RemoveObserver(this);
  }

  void Wait() {
    AppListController::Get()->AddObserver(this);
    run_loop_.Run();
  }

  // AppListControllerObserver:
  void OnAppListVisibilityChanged(bool shown, int64_t display_id) override {
    run_loop_.Quit();
  }

 private:
  base::RunLoop run_loop_;
};

// WindowAddedWaiter -----------------------------------------------------------

// Waits until a child window is added to a container window.
class WindowAddedWaiter : public aura::WindowObserver {
 public:
  explicit WindowAddedWaiter(aura::Window* container) : container_(container) {
    container_->AddObserver(this);
  }
  WindowAddedWaiter(const WindowAddedWaiter&) = delete;
  WindowAddedWaiter& operator=(const WindowAddedWaiter&) = delete;
  ~WindowAddedWaiter() override { container_->RemoveObserver(this); }

  void Wait() { run_loop_.Run(); }

  aura::Window* added_window() { return added_window_; }

 private:
  // aura::WindowObserver:
  void OnWindowAdded(aura::Window* new_window) override {
    added_window_ = new_window;
    DCHECK(run_loop_.IsRunningOnCurrentThread());
    run_loop_.Quit();
  }

  const raw_ptr<aura::Window> container_;
  raw_ptr<aura::Window> added_window_ = nullptr;
  base::RunLoop run_loop_;
};

// ScopedItemMoveAnimationDisabler ---------------------------------------------

// Disable the apps grid item move animation in scope.
class ScopedItemMoveAnimationDisabler {
 public:
  explicit ScopedItemMoveAnimationDisabler(AppsGridView* apps_grid)
      : apps_grid_(apps_grid) {
    DCHECK(!g_disabler_ptr);
    apps_grid_->set_enable_item_move_animation_for_test(false);
    g_disabler_ptr = this;
  }
  ScopedItemMoveAnimationDisabler(const ScopedItemMoveAnimationDisabler&) =
      delete;
  ScopedItemMoveAnimationDisabler& operator=(
      const ScopedItemMoveAnimationDisabler&) = delete;
  ~ScopedItemMoveAnimationDisabler() {
    apps_grid_->set_enable_item_move_animation_for_test(true);
    DCHECK(g_disabler_ptr);
    g_disabler_ptr = nullptr;
  }

 private:
  const raw_ptr<AppsGridView> apps_grid_;
};

}  // namespace

AppListTestApi::AppListTestApi() = default;
AppListTestApi::~AppListTestApi() = default;

AppListModel* AppListTestApi::GetAppListModel() {
  return AppListModelProvider::Get()->model();
}

void AppListTestApi::ShowBubbleAppListAndWait() {
  ash::AcceleratorController::Get()->PerformActionIfEnabled(
      AcceleratorAction::kToggleAppList, {});
  WaitForBubbleWindow(
      /*wait_for_opening_animation=*/true);
}

void AppListTestApi::WaitForBubbleWindow(bool wait_for_opening_animation) {
  WaitForBubbleWindowInRootWindow(Shell::GetPrimaryRootWindow(),
                                  wait_for_opening_animation);
}

void AppListTestApi::WaitForBubbleWindowInRootWindow(
    aura::Window* root_window,
    bool wait_for_opening_animation) {
  DCHECK(!Shell::Get()->IsInTabletMode());

  // Wait for the window only when the app list window does not exist.
  auto* app_list_controller = Shell::Get()->app_list_controller();
  if (!app_list_controller->GetWindow()) {
    // Wait for a child window to be added to the app list container.
    aura::Window* container =
        Shell::GetContainer(root_window, kShellWindowId_AppListContainer);
    WindowAddedWaiter waiter(container);
    waiter.Wait();

    // App list window exists.
    aura::Window* app_list_window = app_list_controller->GetWindow();
    DCHECK(app_list_window);
    DCHECK_EQ(app_list_window, waiter.added_window());
  }

  if (wait_for_opening_animation)
    WaitForAppListShowAnimation(/*is_bubble_window=*/true);
}

void AppListTestApi::WaitForAppListShowAnimation(bool is_bubble_window) {
  // Ensure that the app list is visible before waiting for animations.
  AppListController* controller = AppListControllerImpl::Get();
  if (!controller->IsVisible()) {
    AppListVisibilityChangedWaiter waiter;
    waiter.Wait();
    if (!controller->IsVisible())
      ADD_FAILURE() << "Launcher is not visible.";
  }

  // Wait for the app list window animation.
  aura::Window* app_list_window = controller->GetWindow();
  DCHECK(app_list_window);
  ui::LayerAnimationStoppedWaiter().Wait(app_list_window->layer());

  if (!is_bubble_window)
    return;

  DCHECK(!Shell::Get()->IsInTabletMode());

  ScrollableAppsGridView* scrollable_apps_grid_view =
      static_cast<ScrollableAppsGridView*>(GetTopLevelAppsGridView());
  if (!scrollable_apps_grid_view->layer())
    return;

  // Wait for the animation to show the bubble view.
  ui::LayerAnimationStoppedWaiter().Wait(GetAppListBubbleView()->layer());

  // Wait for the animation to show the apps page.
  ui::LayerAnimationStoppedWaiter().Wait(GetAppListBubbleView()
                                             ->apps_page_for_test()
                                             ->scroll_view()
                                             ->contents()
                                             ->layer());

  // Wait for the apps grid slide animation.
  ui::LayerAnimationStoppedWaiter().Wait(scrollable_apps_grid_view->layer());
}

bool AppListTestApi::HasApp(const std::string& app_id) {
  return GetAppListModel()->FindItem(app_id);
}

std::u16string AppListTestApi::GetAppListItemViewName(
    const std::string& item_id) {
  AppListItemView* item_view = GetTopLevelItemViewFromId(item_id);
  if (!item_view)
    return u"";

  return item_view->title()->GetText();
}

AppListItemView* AppListTestApi::GetTopLevelItemViewFromId(
    const std::string& item_id) {
  views::ViewModelT<AppListItemView>* view_model =
      GetTopLevelAppsGridView()->view_model();
  for (size_t i = 0; i < view_model->view_size(); ++i) {
    AppListItemView* app_list_item_view = view_model->view_at(i);
    if (app_list_item_view->item()->id() == item_id)
      return app_list_item_view;
  }

  return nullptr;
}

std::vector<std::string> AppListTestApi::GetTopLevelViewIdList() {
  std::vector<std::string> id_list;
  auto* view_model = GetTopLevelAppsGridView()->view_model();
  for (size_t i = 0; i < view_model->view_size(); ++i) {
    AppListItem* app_list_item = view_model->view_at(i)->item();
    if (app_list_item) {
      id_list.push_back(app_list_item->id());
    }
  }
  return id_list;
}

std::string AppListTestApi::CreateFolderWithApps(
    const std::vector<std::string>& apps) {
  // Only create a folder if there are two or more apps.
  DCHECK_GE(apps.size(), 2u);

  // Skip all item move animations during folder creation.
  ScopedItemMoveAnimationDisabler disabler(GetTopLevelAppsGridView());

  AppListModel* model = GetAppListModel();
  // Create a folder using the first two apps, and add the others to the
  // folder iteratively.
  std::string folder_id = model->MergeItems(apps[0], apps[1]);
  // Return early if MergeItems failed.
  if (folder_id.empty())
    return "";

  for (size_t i = 2; i < apps.size(); ++i)
    model->MergeItems(folder_id, apps[i]);
  return folder_id;
}

std::string AppListTestApi::GetFolderId(const std::string& app_id) {
  return GetAppListModel()->FindItem(app_id)->folder_id();
}

std::vector<std::string> AppListTestApi::GetAppIdsInFolder(
    const std::string& folder_id) {
  AppListItem* folder_item = GetAppListModel()->FindItem(folder_id);
  DCHECK(folder_item->is_folder());
  AppListItemList* folder_list =
      static_cast<AppListFolderItem*>(folder_item)->item_list();
  std::vector<std::string> id_list;
  for (size_t i = 0; i < folder_list->item_count(); ++i)
    id_list.push_back(folder_list->item_at(i)->id());
  return id_list;
}

void AppListTestApi::MoveItemToPosition(const std::string& item_id,
                                        const size_t to_index) {
  AppListItem* app_item = GetAppListModel()->FindItem(item_id);
  const std::string folder_id = app_item->folder_id();

  AppListItemList* item_list;
  std::vector<std::string> top_level_id_list = GetTopLevelViewIdList();
  // The app should be either at the top level or in a folder.
  if (folder_id.empty()) {
    // The app is at the top level.
    item_list = GetAppListModel()->top_level_item_list();
  } else {
    // The app is in the folder with |folder_id|.
    item_list = GetAppListModel()->FindFolderItem(folder_id)->item_list();
  }
  size_t from_index = 0;
  item_list->FindItemIndex(item_id, &from_index);
  item_list->MoveItem(from_index, to_index);
}

int AppListTestApi::GetTopListItemCount() {
  return GetAppListModel()->top_level_item_list()->item_count();
}

views::View* AppListTestApi::GetLastItemInAppsGridView() {
  AppsGridView* grid = GetTopLevelAppsGridView();
  return grid->view_model()->view_at(grid->view_model()->view_size() - 1);
}

PaginationModel* AppListTestApi::GetPaginationModel() {
  return GetPagedAppsGridView()->pagination_model();
}

AppsGridView* AppListTestApi::GetTopLevelAppsGridView() {
  if (ShouldUseBubbleAppList()) {
    return GetAppListBubbleView()
        ->apps_page_for_test()
        ->scrollable_apps_grid_view();
  }

  return GetPagedAppsGridView();
}

const AppsGridView* AppListTestApi::GetTopLevelAppsGridView() const {
  if (ShouldUseBubbleAppList()) {
    return GetAppListBubbleView()
        ->apps_page_for_test()
        ->scrollable_apps_grid_view();
  }

  return GetPagedAppsGridView();
}

AppsGridView* AppListTestApi::GetFolderAppsGridView() {
  return GetAppListFolderView()->items_grid_view();
}

bool AppListTestApi::IsFolderViewAnimating() const {
  return GetAppListFolderView()->IsAnimationRunning();
}

views::View* AppListTestApi::GetBubbleReorderUndoButton() {
  return GetToastContainerViewFromBubble()->GetToastButton();
}

views::View* AppListTestApi::GetFullscreenReorderUndoButton() {
  return GetToastContainerViewFromFullscreenAppList()->GetToastButton();
}

AppListToastType AppListTestApi::GetToastType() const {
  AppListToastContainerView* toast_container =
      ShouldUseBubbleAppList() ? GetToastContainerViewFromBubble()
                               : GetToastContainerViewFromFullscreenAppList();
  return toast_container->current_toast();
}

void AppListTestApi::SetFolderViewAnimationCallback(
    base::OnceClosure folder_animation_done_callback) {
  AppListFolderView* folder_view = GetAppListFolderView();
  folder_view->SetAnimationDoneTestCallback(base::BindOnce(
      [](AppListFolderView* folder_view,
         base::OnceClosure folder_animation_done_callback) {
        std::move(folder_animation_done_callback).Run();
      },
      folder_view, std::move(folder_animation_done_callback)));
}

views::View* AppListTestApi::GetToastContainerView() {
  if (ShouldUseBubbleAppList())
    return GetToastContainerViewFromBubble();

  return GetToastContainerViewFromFullscreenAppList();
}

void AppListTestApi::AddReorderAnimationCallback(
    AppsGridView::TestReorderDoneCallbackType callback) {
  GetTopLevelAppsGridView()->AddReorderCallbackForTest(std::move(callback));
}

void AppListTestApi::AddFadeOutAnimationStartClosure(
    base::OnceClosure closure) {
  GetTopLevelAppsGridView()->AddFadeOutAnimationStartClosureForTest(
      std::move(closure));
}

bool AppListTestApi::HasAnyWaitingReorderDoneCallback() const {
  return GetTopLevelAppsGridView()->HasAnyWaitingReorderDoneCallbackForTest();
}

void AppListTestApi::DisableAppListNudge(bool disable) {
  AppListNudgeController::SetReorderNudgeDisabledForTest(disable);
}

void AppListTestApi::SetContinueSectionPrivacyNoticeAccepted() {
  AppListNudgeController::SetPrivacyNoticeAcceptedForTest(true);
}

void AppListTestApi::ReorderItemInRootByDragAndDrop(int source_index,
                                                    int target_index) {
  test::AppsGridViewTestApi(GetTopLevelAppsGridView())
      .ReorderItemByDragAndDrop(source_index, target_index);
}

views::View* AppListTestApi::GetVisibleSearchResultView(int index) {
  views::View* app_list =
      ShouldUseBubbleAppList()
          ? static_cast<views::View*>(GetAppListBubbleView())
          : static_cast<views::View*>(GetAppListView());

  views::View::Views search_results;
  app_list->GetViewsInGroup(kSearchResultViewGroup, &search_results);

  int current_visible_index = -1;
  for (views::View* view : search_results) {
    if (view->GetVisible())
      ++current_visible_index;
    if (current_visible_index == index)
      return view;
  }
  return nullptr;
}

ash::AppListItemView* AppListTestApi::FindTopLevelFolderItemView() {
  return FindFolderItemView(GetTopLevelAppsGridView());
}

void AppListTestApi::VerifyTopLevelItemVisibility() {
  auto* view_model = GetTopLevelAppsGridView()->view_model();
  std::vector<std::string> invisible_item_names;
  for (size_t view_index = 0; view_index < view_model->view_size();
       ++view_index) {
    auto* item_view = view_model->view_at(view_index);
    if (!item_view->GetVisible())
      invisible_item_names.push_back(item_view->item()->name());
  }

  // Invisible items should be none.
  EXPECT_EQ(std::vector<std::string>(), invisible_item_names);
}

views::View* AppListTestApi::GetRecentAppAt(int index) {
  return GetRecentAppsView()->GetItemViewAt(index);
}

std::vector<ContinueTaskView*> AppListTestApi::GetContinueTaskViews() {
  std::vector<ContinueTaskView*> results;
  ContinueSectionView* const container = GetContinueSectionView();
  for (size_t i = 0; i < container->GetTasksSuggestionsCount(); ++i) {
    results.push_back(container->GetTaskViewAtForTesting(i));
  }
  return results;
}

std::vector<std::string> AppListTestApi::GetRecentAppIds() {
  std::vector<std::string> ids;
  RecentAppsView* recent_apps = GetRecentAppsView();
  for (int i = 0; i < recent_apps->GetItemViewCount(); ++i) {
    ids.push_back(recent_apps->GetItemViewAt(i)->item()->id());
  }
  return ids;
}

void AppListTestApi::SimulateSearch(const std::u16string& query) {
  views::Textfield* textfield = GetSearchBoxView()->search_box();
  textfield->SetText(u"");
  textfield->InsertText(
      query,
      ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
}

SearchResultListView* AppListTestApi::GetTopVisibleSearchResultListView() {
  std::vector<raw_ptr<SearchResultContainerView, VectorExperimental>>
      result_containers = GetSearchView()->result_container_views_for_test();
  // Check that one of the `result_containers` is kApps.
  for (SearchResultContainerView* container : result_containers) {
    SearchResultListView* list_view =
        views::AsViewClass<SearchResultListView>(container);
    if (list_view && list_view->GetVisible()) {
      return list_view;
    }
  }
  return nullptr;
}

void AppListTestApi::ReorderByMouseClickAtContextMenuInAppsGrid(
    ash::AppsGridView* apps_grid_view,
    ash::AppListSortOrder order,
    MenuType menu_type,
    ui::test::EventGenerator* event_generator,
    ReorderAnimationEndState target_state,
    ReorderAnimationEndState* actual_state) {
  // Ensure that the apps grid layout is refreshed before showing the
  // context menu.
  apps_grid_view->GetWidget()->LayoutRootViewIfNecessary();

  // Custom order is not a menu option.
  EXPECT_NE(order, ash::AppListSortOrder::kCustom);

  views::MenuItemView* root_menu =
      ShowRootMenuAndReturn(apps_grid_view, menu_type, event_generator);

  // Get the "Name" or "Color" option.
  views::MenuItemView* reorder_option = nullptr;
  switch (menu_type) {
    case MenuType::kAppListPageMenu:
    case MenuType::kAppListFolderItemMenu:
      reorder_option =
          GetReorderOptionForAppListOrFolderItemMenu(root_menu, order);
      break;
    case MenuType::kAppListNonFolderItemMenu: {
      // The `reorder_option` cached here is the submenu of the options.
      views::MenuItemView* reorder_submenu =
          GetReorderOptionForNonFolderItemMenu(root_menu, order);
      event_generator->MoveMouseTo(
          reorder_submenu->GetBoundsInScreen().CenterPoint());
      event_generator->ClickLeftButton();
      reorder_option = reorder_submenu->GetSubmenu()->GetMenuItemAt(
          GetMenuIndexOfSortingOrder(order));
      break;
    }
  }

  gfx::Point point_on_option =
      reorder_option->GetBoundsInScreen().CenterPoint();

  RegisterReorderAnimationDoneCallback(actual_state);

  // Click at the sorting option.
  event_generator->MoveMouseTo(point_on_option);
  event_generator->ClickLeftButton();

  switch (target_state) {
    case ReorderAnimationEndState::kCompleted:
      // Wait until the reorder animation is done.
      WaitForReorderAnimationAndVerifyItemVisibility();
      break;
    case ReorderAnimationEndState::kFadeOutAborted:
      // The fade out animation starts synchronously so do not wait before
      // animation interruption.
      break;
    case ReorderAnimationEndState::kFadeInAborted:
      // Wait until the fade out animation is done. It ensures that the app
      // list is under fade in animation when animation interruption occurs.
      WaitForFadeOutAnimation();
      break;
  }
}

void AppListTestApi::ReorderByMouseClickAtToplevelAppsGridMenu(
    ash::AppListSortOrder order,
    MenuType menu_type,
    ui::test::EventGenerator* event_generator,
    ReorderAnimationEndState target_state,
    ReorderAnimationEndState* actual_state) {
  ReorderByMouseClickAtContextMenuInAppsGrid(GetTopLevelAppsGridView(), order,
                                             menu_type, event_generator,
                                             target_state, actual_state);
}

void AppListTestApi::ClickOnRedoButtonAndWaitForAnimation(
    ui::test::EventGenerator* event_generator) {
  ReorderAnimationEndState actual_state;
  RegisterReorderAnimationDoneCallback(&actual_state);

  // Mouse click at the undo button.
  views::View* reorder_undo_toast_button = nullptr;
  if (ShouldUseBubbleAppList())
    reorder_undo_toast_button = GetBubbleReorderUndoButton();
  else
    reorder_undo_toast_button = GetFullscreenReorderUndoButton();
  event_generator->MoveMouseTo(
      reorder_undo_toast_button->GetBoundsInScreen().CenterPoint());
  event_generator->ClickLeftButton();

  // Verify that the toast is under animation.
  EXPECT_TRUE(GetToastContainerView()->layer()->GetAnimator()->is_animating());

  WaitForReorderAnimationAndVerifyItemVisibility();
  EXPECT_EQ(ReorderAnimationEndState::kCompleted, actual_state);
}

void AppListTestApi::ClickOnCloseButtonAndWaitForToastAnimation(
    ui::test::EventGenerator* event_generator) {
  AppListToastContainerView* toast_container =
      ShouldUseBubbleAppList() ? GetToastContainerViewFromBubble()
                               : GetToastContainerViewFromFullscreenAppList();
  views::View* close_button = toast_container->GetCloseButton();
  event_generator->MoveMouseTo(close_button->GetBoundsInScreen().CenterPoint());
  event_generator->ClickLeftButton();

  // Wait until the toast fade out animation ends.
  ui::LayerAnimationStoppedWaiter animation_waiter;
  animation_waiter.Wait(toast_container->toast_view()->layer());
}

ui::Layer* AppListTestApi::GetAppListViewLayer() {
  return GetAppListView()->GetWidget()->GetNativeView()->layer();
}

void AppListTestApi::RegisterReorderAnimationDoneCallback(
    ReorderAnimationEndState* actual_state) {
  AddReorderAnimationCallback(base::BindRepeating(
      &AppListTestApi::OnReorderAnimationDone, weak_factory_.GetWeakPtr(),
      !ash::Shell::Get()->IsInTabletMode(), actual_state));
}

void AppListTestApi::OnReorderAnimationDone(bool for_bubble_app_list,
                                            ReorderAnimationEndState* result,
                                            bool abort,
                                            AppListGridAnimationStatus status) {
  DCHECK(status == AppListGridAnimationStatus::kReorderFadeOut ||
         status == AppListGridAnimationStatus::kReorderFadeIn);

  // Record the animation running result.
  if (abort) {
    if (status == AppListGridAnimationStatus::kReorderFadeOut)
      *result = ReorderAnimationEndState::kFadeOutAborted;
    else
      *result = ReorderAnimationEndState::kFadeInAborted;
  } else {
    EXPECT_EQ(AppListGridAnimationStatus::kReorderFadeIn, status);
    *result = ReorderAnimationEndState::kCompleted;

    // Verify that the toast container under the clamshell mode does not have
    // a layer after reorder animation completes.
    if (for_bubble_app_list)
      EXPECT_FALSE(GetToastContainerView()->layer());
  }

  // Callback can be registered without a running loop.
  if (run_loop_for_reorder_)
    run_loop_for_reorder_->Quit();
}

void AppListTestApi::WaitForReorderAnimationAndVerifyItemVisibility() {
  run_loop_for_reorder_ = std::make_unique<base::RunLoop>();
  run_loop_for_reorder_->Run();

  VerifyTopLevelItemVisibility();
}

void AppListTestApi::WaitForFadeOutAnimation() {
  ash::AppsGridView* apps_grid_view = GetTopLevelAppsGridView();

  if (apps_grid_view->grid_animation_status_for_test() !=
      AppListGridAnimationStatus::kReorderFadeOut) {
    // The apps grid is not under fade out animation so no op.
    return;
  }

  ASSERT_TRUE(!run_loop_for_reorder_ || !run_loop_for_reorder_->running());
  run_loop_for_reorder_ = std::make_unique<base::RunLoop>();
  apps_grid_view->AddFadeOutAnimationDoneClosureForTest(
      run_loop_for_reorder_->QuitClosure());
  run_loop_for_reorder_->Run();
}

}  // namespace ash
