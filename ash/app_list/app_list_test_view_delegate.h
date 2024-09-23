// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_APP_LIST_TEST_VIEW_DELEGATE_H_
#define ASH_APP_LIST_APP_LIST_TEST_VIEW_DELEGATE_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/app_list/app_list_model_provider.h"
#include "ash/app_list/app_list_view_delegate.h"
#include "ash/app_list/model/app_list_test_model.h"
#include "ash/app_list/model/search/search_model.h"
#include "ash/app_list/quick_app_access_model.h"
#include "ash/public/cpp/app_list/app_list_client.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/functional/callback_forward.h"
#include "chromeos/ash/services/assistant/public/cpp/assistant_enums.h"
#include "ui/base/models/simple_menu_model.h"

namespace ash {
namespace test {

class AppListTestModel;

// A concrete AppListViewDelegate for unit tests.
class AppListTestViewDelegate : public AppListViewDelegate,
                                public ui::SimpleMenuModel::Delegate {
 public:
  AppListTestViewDelegate();

  AppListTestViewDelegate(const AppListTestViewDelegate&) = delete;
  AppListTestViewDelegate& operator=(const AppListTestViewDelegate&) = delete;

  ~AppListTestViewDelegate() override;

  int dismiss_count() const { return dismiss_count_; }
  int open_search_result_count() const { return open_search_result_count_; }
  std::map<size_t, int>& open_search_result_counts() {
    return open_search_result_counts_;
  }
  int show_wallpaper_context_menu_count() const {
    return show_wallpaper_context_menu_count_;
  }

  // Sets the number of apps that the model will be created with the next time
  // SetProfileByPath() is called.
  void set_next_profile_app_count(int apps) { next_profile_app_count_ = apps; }

  // Sets whether the search engine is Google or not.
  void SetSearchEngineIsGoogle(bool is_google);

  // Set whether tablet mode is enabled.
  void SetIsTabletModeEnabled(bool is_tablet_mode);

  // AppListViewDelegate overrides:
  bool KeyboardTraversalEngaged() override;
  void StartAssistant(assistant::AssistantEntryPoint entry_point) override {}
  void EndAssistant(assistant::AssistantExitPoint exit_point) override {}
  std::vector<AppListSearchControlCategory> GetToggleableCategories()
      const override;
  void StartSearch(const std::u16string& raw_query) override {}
  void StartZeroStateSearch(base::OnceClosure callback,
                            base::TimeDelta timeout) override;
  void OpenSearchResult(const std::string& result_id,
                        int event_flags,
                        ash::AppListLaunchedFrom launched_from,
                        ash::AppListLaunchType launch_type,
                        int suggestion_index,
                        bool launch_as_default) override;
  void InvokeSearchResultAction(const std::string& result_id,
                                SearchResultActionType action) override {}
  void ViewShown(int64_t display_id) override {}
  void DismissAppList() override;
  void ViewClosing() override {}
  void ActivateItem(const std::string& id,
                    int event_flags,
                    ash::AppListLaunchedFrom launched_from,
                    bool is_app_above_the_fold) override;
  void GetContextMenuModel(const std::string& id,
                           AppListItemContext item_context,
                           GetContextMenuModelCallback callback) override;
  void ShowWallpaperContextMenu(const gfx::Point& onscreen_location,
                                ui::MenuSourceType source_type) override;
  bool CanProcessEventsOnApplistViews() override;
  bool ShouldDismissImmediately() override;
  ash::AssistantViewDelegate* GetAssistantViewDelegate() override;
  void OnSearchResultVisibilityChanged(const std::string& id,
                                       bool visibility) override;
  bool IsAssistantAllowedAndEnabled() const override;
  void OnStateTransitionAnimationCompleted(
      AppListViewState state,
      bool was_animation_interrupted) override;
  AppListState GetCurrentAppListPage() const override;
  void OnAppListPageChanged(AppListState page) override;
  AppListViewState GetAppListViewState() const override;
  void OnViewStateChanged(AppListViewState state) override;
  void GetAppLaunchedMetricParams(
      AppLaunchedMetricParams* metric_params) override;
  gfx::Rect SnapBoundsToDisplayEdge(const gfx::Rect& bounds) override;
  int GetShelfSize() override;
  int GetSystemShelfInsetsInTabletMode() override;
  bool AppListTargetVisibility() const override;
  bool IsInTabletMode() const override;
  AppListNotifier* GetNotifier() override;
  std::unique_ptr<ScopedIphSession> CreateLauncherSearchIphSession() override;
  void LoadIcon(const std::string& app_id) override {}
  bool HasValidProfile() const override;
  bool ShouldHideContinueSection() const override;
  void SetHideContinueSection(bool hide) override;
  bool IsCategoryEnabled(AppListSearchControlCategory category) override;
  void SetCategoryEnabled(AppListSearchControlCategory category,
                          bool enabled) override {}
  void RecordAppsDefaultVisibility(
      const std::vector<std::string>& apps_above_the_fold,
      const std::vector<std::string>& apps_below_the_fold,
      bool is_apps_collections_page) override {}

  // Do a bulk replacement of the items in the model.
  void ReplaceTestModel(int item_count);

  AppListTestModel* ReleaseTestModel() { return model_.release(); }
  AppListTestModel* GetTestModel() { return model_.get(); }

  SearchModel* ReleaseTestSearchModel() { return search_model_.release(); }

 private:
  void RecordAppLaunched(ash::AppListLaunchedFrom launched_from);

  // ui::SimpleMenuModel::Delegate overrides:
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;

  int dismiss_count_ = 0;
  int open_search_result_count_ = 0;
  int next_profile_app_count_ = 0;
  int show_wallpaper_context_menu_count_ = 0;
  AppListState app_list_page_ = AppListState::kInvalidState;
  AppListViewState app_list_view_state_ = AppListViewState::kClosed;
  bool is_tablet_mode_ = false;
  std::map<size_t, int> open_search_result_counts_;
  AppListModelProvider model_provider_;
  std::unique_ptr<AppListTestModel> model_;
  std::unique_ptr<SearchModel> search_model_;
  std::unique_ptr<QuickAppAccessModel> quick_app_access_model_;
};

}  // namespace test
}  // namespace ash

#endif  // ASH_APP_LIST_APP_LIST_TEST_VIEW_DELEGATE_H_
