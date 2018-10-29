// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_TEST_APP_LIST_TEST_VIEW_DELEGATE_H_
#define ASH_APP_LIST_TEST_APP_LIST_TEST_VIEW_DELEGATE_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "ash/app_list/app_list_view_delegate.h"
#include "ash/app_list/model/search/search_model.h"
#include "ash/app_list/test/app_list_test_model.h"
#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "ui/base/models/simple_menu_model.h"

namespace app_list {
namespace test {

class AppListTestModel;

// A concrete AppListViewDelegate for unit tests.
class AppListTestViewDelegate : public AppListViewDelegate,
                                public ui::SimpleMenuModel::Delegate {
 public:
  AppListTestViewDelegate();
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

  // AppListViewDelegate overrides:
  AppListModel* GetModel() override;
  SearchModel* GetSearchModel() override;
  void StartAssistant() override {}
  void StartSearch(const base::string16& raw_query) override {}
  void OpenSearchResult(const std::string& result_id, int event_flags) override;
  void InvokeSearchResultAction(const std::string& result_id,
                                int action_index,
                                int event_flags) override {}
  void GetSearchResultContextMenuModel(
      const std::string& result_id,
      GetContextMenuModelCallback callback) override;
  void SearchResultContextMenuItemSelected(const std::string& result_id,
                                           int command_id,
                                           int event_flags) override {}
  void ViewShown(int64_t display_id) override {}
  void DismissAppList() override;
  void ViewClosing() override {}
  void ViewClosed() override {}
  void GetWallpaperProminentColors(
      GetWallpaperProminentColorsCallback callback) override {}
  void ActivateItem(const std::string& id, int event_flags) override;
  void GetContextMenuModel(const std::string& id,
                           GetContextMenuModelCallback callback) override;
  void ContextMenuItemSelected(const std::string& id,
                               int command_id,
                               int event_flags) override {}
  void ShowWallpaperContextMenu(const gfx::Point& onscreen_location,
                                ui::MenuSourceType source_type) override;
  bool ProcessHomeLauncherGesture(ui::GestureEvent* event,
                                  const gfx::Point& screen_location) override;
  bool CanProcessEventsOnApplistViews() override;
  ws::WindowService* GetWindowService() override;

  // Do a bulk replacement of the items in the model.
  void ReplaceTestModel(int item_count);

  AppListTestModel* ReleaseTestModel() { return model_.release(); }
  AppListTestModel* GetTestModel() { return model_.get(); }

 private:
  // ui::SimpleMenuModel::Delegate overrides:
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;

  int dismiss_count_ = 0;
  int open_search_result_count_ = 0;
  int next_profile_app_count_ = 0;
  int show_wallpaper_context_menu_count_ = 0;
  std::map<size_t, int> open_search_result_counts_;
  std::unique_ptr<AppListTestModel> model_;
  std::unique_ptr<SearchModel> search_model_;
  std::vector<SkColor> wallpaper_prominent_colors_;
  ui::SimpleMenuModel search_result_context_menu_model_;

  DISALLOW_COPY_AND_ASSIGN(AppListTestViewDelegate);
};

}  // namespace test
}  // namespace app_list

#endif  // ASH_APP_LIST_TEST_APP_LIST_TEST_VIEW_DELEGATE_H_
