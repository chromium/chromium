// Copyright 2020 The Chromium Authors. All rights reserved.
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
#include "ash/app_list/model/app_list_folder_item.h"
#include "ash/app_list/model/app_list_item.h"
#include "ash/app_list/model/app_list_model.h"
#include "ash/app_list/views/app_list_bubble_apps_page.h"
#include "ash/app_list/views/app_list_bubble_view.h"
#include "ash/app_list/views/app_list_folder_view.h"
#include "ash/app_list/views/app_list_item_view.h"
#include "ash/app_list/views/app_list_main_view.h"
#include "ash/app_list/views/app_list_toast_container_view.h"
#include "ash/app_list/views/app_list_view.h"
#include "ash/app_list/views/apps_container_view.h"
#include "ash/app_list/views/apps_grid_view.h"
#include "ash/app_list/views/apps_grid_view_test_api.h"
#include "ash/app_list/views/contents_view.h"
#include "ash/app_list/views/paged_apps_grid_view.h"
#include "ash/app_list/views/scrollable_apps_grid_view.h"
#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "ash/test/layer_animation_stopped_waiter.h"
#include "base/callback.h"
#include "base/run_loop.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window_observer.h"
#include "ui/compositor/layer.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/view_model.h"

namespace ash {

namespace {

// A global pointer to the disabler's instance. Used to ensure at most one
// disabler exists at a time.
class ScopedItemMoveAnimationDisabler;
ScopedItemMoveAnimationDisabler* g_disabler_ptr = nullptr;

AppListView* GetAppListView() {
  return Shell::Get()->app_list_controller()->fullscreen_presenter()->GetView();
}

// An app list should be either a bubble app list or a fullscreen app list.
// Returns true if a bubble app list should be used under the current mode.
bool ShouldUseBubbleAppList() {
  // A bubble app list should be used only when:
  // (1) It is in clamshell mode; and
  // (2) The productivity launcher flag is enabled.
  return !Shell::Get()->IsInTabletMode() &&
         features::IsProductivityLauncherEnabled();
}

PagedAppsGridView* GetPagedAppsGridView() {
  // This view only exists for tablet launcher and legacy peeking launcher.
  DCHECK(!ShouldUseBubbleAppList());
  return AppListView::TestApi(GetAppListView()).GetRootAppsGridView();
}

AppListBubbleView* GetAppListBubbleView() {
  AppListBubbleView* bubble_view = Shell::Get()
                                       ->app_list_controller()
                                       ->bubble_presenter_for_test()
                                       ->bubble_view_for_test();
  DCHECK(bubble_view) << "Bubble launcher view not yet created. Tests must "
                         "show the launcher and may need to call "
                         "WaitForBubbleWindow() if animations are enabled.";
  return bubble_view;
}

AppListFolderView* GetAppListFolderView() {
  // Handle the case that the app list bubble view is effective.
  if (ShouldUseBubbleAppList())
    return GetAppListBubbleView()->folder_view_for_test();

  return GetAppListView()
      ->app_list_main_view()
      ->contents_view()
      ->apps_container_view()
      ->app_list_folder_view();
}

AppListToastContainerView* GetToastContainerViewFromBubble() {
  DCHECK(features::IsLauncherAppSortEnabled());
  return GetAppListBubbleView()
      ->apps_page_for_test()
      ->toast_container_for_test();
}

AppListToastContainerView* GetToastContainerViewFromFullscreenAppList() {
  DCHECK(features::IsLauncherAppSortEnabled());
  return GetAppListView()
      ->app_list_main_view()
      ->contents_view()
      ->apps_container_view()
      ->toast_container_for_test();
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

  aura::Window* const container_;
  aura::Window* added_window_ = nullptr;
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
  AppsGridView* const apps_grid_;
};

}  // namespace

AppListTestApi::AppListTestApi() = default;
AppListTestApi::~AppListTestApi() = default;

AppListModel* AppListTestApi::GetAppListModel() {
  return AppListModelProvider::Get()->model();
}

void AppListTestApi::WaitForBubbleWindow(bool wait_for_opening_animation) {
  DCHECK(features::IsProductivityLauncherEnabled());
  DCHECK(!Shell::Get()->IsInTabletMode());

  // Wait for the window only when the app list window does not exist.
  auto* app_list_controller = Shell::Get()->app_list_controller();
  if (!app_list_controller->GetWindow()) {
    // Wait for a child window to be added to the app list container.
    aura::Window* container = Shell::GetContainer(
        Shell::GetPrimaryRootWindow(), kShellWindowId_AppListContainer);
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
  LayerAnimationStoppedWaiter().Wait(app_list_window->layer());

  if (!is_bubble_window)
    return;

  DCHECK(features::IsProductivityLauncherEnabled());
  DCHECK(!Shell::Get()->IsInTabletMode());

  ScrollableAppsGridView* scrollable_apps_grid_view =
      static_cast<ScrollableAppsGridView*>(GetTopLevelAppsGridView());
  if (!scrollable_apps_grid_view->layer())
    return;

  // Wait for the animation to show the bubble view.
  LayerAnimationStoppedWaiter().Wait(GetAppListBubbleView()->layer());

  // Wait for the animation to show the apps page.
  LayerAnimationStoppedWaiter().Wait(GetAppListBubbleView()
                                         ->apps_page_for_test()
                                         ->scroll_view()
                                         ->contents()
                                         ->layer());

  // Wait for the apps grid slide animation.
  LayerAnimationStoppedWaiter().Wait(scrollable_apps_grid_view->layer());
}

bool AppListTestApi::HasApp(const std::string& app_id) {
  return GetAppListModel()->FindItem(app_id);
}

std::u16string AppListTestApi::GetAppListItemViewName(
    const std::string& item_id) {
  views::ViewModelT<AppListItemView>* view_model =
      GetTopLevelAppsGridView()->view_model();
  for (int i = 0; i < view_model->view_size(); ++i) {
    AppListItemView* app_list_item_view = view_model->view_at(i);
    if (app_list_item_view->item()->id() == item_id)
      return app_list_item_view->title()->GetText();
  }
  return u"";
}

std::vector<std::string> AppListTestApi::GetTopLevelViewIdList() {
  std::vector<std::string> id_list;
  auto* view_model = GetTopLevelAppsGridView()->view_model();
  for (int i = 0; i < view_model->view_size(); ++i) {
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

  AppListModel* model = GetAppListModel();
  // Create a folder using the first two apps, and add the others to the
  // folder iteratively.
  std::string folder_id = model->MergeItems(apps[0], apps[1]);
  // Return early if MergeItems failed.
  if (folder_id.empty())
    return "";

  // Skip item move animations.
  ScopedItemMoveAnimationDisabler disabler(GetTopLevelAppsGridView());

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

void AppListTestApi::AddPageBreakItemAfterId(const std::string& item_id) {
  auto* model = GetAppListModel();
  model->AddPageBreakItemAfter(model->FindItem(item_id));
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

void AppListTestApi::UpdatePagedViewStructure() {
  GetPagedAppsGridView()->UpdatePagedViewStructure();
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
  return GetToastContainerViewFromBubble()->GetToastDismissButtonForTest();
}

views::View* AppListTestApi::GetFullscreenReorderUndoButton() {
  return GetToastContainerViewFromFullscreenAppList()
      ->GetToastDismissButtonForTest();
}

bool AppListTestApi::GetBubbleReorderUndoToastVisibility() const {
  return GetToastContainerViewFromBubble()->is_toast_visible();
}

bool AppListTestApi::GetFullscreenReorderUndoToastVisibility() const {
  return GetToastContainerViewFromFullscreenAppList()->is_toast_visible();
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
  DCHECK(features::IsLauncherAppSortEnabled());
  GetTopLevelAppsGridView()->AddReorderCallbackForTest(std::move(callback));
}

void AppListTestApi::AddFadeOutAnimationStartClosure(
    base::OnceClosure closure) {
  DCHECK(features::IsLauncherAppSortEnabled());
  GetTopLevelAppsGridView()->AddFadeOutAnimationStartClosureForTest(
      std::move(closure));
}

bool AppListTestApi::HasAnyWaitingReorderDoneCallback() const {
  DCHECK(features::IsLauncherAppSortEnabled());
  return GetTopLevelAppsGridView()->HasAnyWaitingReorderDoneCallbackForTest();
}

void AppListTestApi::DisableAppListNudge(bool disable) {
  AppListNudgeController::SetReorderNudgeDisabledForTest(disable);
}

void AppListTestApi::ReorderItemInRootByDragAndDrop(int source_index,
                                                    int target_index) {
  test::AppsGridViewTestApi(GetTopLevelAppsGridView())
      .ReorderItemByDragAndDrop(source_index, target_index);
}

}  // namespace ash
