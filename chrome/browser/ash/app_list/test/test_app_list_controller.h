// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_TEST_TEST_APP_LIST_CONTROLLER_H_
#define CHROME_BROWSER_ASH_APP_LIST_TEST_TEST_APP_LIST_CONTROLLER_H_

#include "ash/public/cpp/app_list/app_list_controller.h"

#include "base/observer_list.h"

namespace ash {
class AppListControllerObserver;
}

namespace test {

// A fake app list controller used by browser side unit tests to emulate the
// interaction between browser and ash in tests.
// Currently, it only tracks app list visibility updates using ShowAppList() and
// DismissAppList().
class TestAppListController : public ash::AppListController {
 public:
  TestAppListController();
  TestAppListController(const TestAppListController&) = delete;
  TestAppListController& operator=(const TestAppListController&) = delete;
  ~TestAppListController() override;

  // ash::AppListController:
  void SetClient(ash::AppListClient* client) override {}
  ash::AppListClient* GetClient() override;
  void AddObserver(ash::AppListControllerObserver* observer) override;
  void RemoveObserver(ash::AppListControllerObserver* obsever) override;
  void SetActiveModel(
      int profile_id,
      ash::AppListModel* model,
      ash::SearchModel* search_model,
      ash::QuickAppAccessModel* quick_app_access_model) override {}
  void ClearActiveModel() override {}
  void ShowAppList(ash::AppListShowSource source) override;
  ash::AppListShowSource LastAppListShowSource() override;
  void DismissAppList() override;
  aura::Window* GetWindow() override;
  bool IsVisible(const std::optional<int64_t>& display_id) override;
  bool IsVisible() override;
  void UpdateAppListWithNewTemporarySortOrder(
      const std::optional<ash::AppListSortOrder>& new_order,
      bool animate,
      base::OnceClosure update_position_closure) override;

 private:
  void NotifyAppListVisibilityChanged();

  // The visibility state set using (Show|Dismiss)AppList.
  bool visible_ = false;

  // Tracks the most recent show source for the app list.
  std::optional<ash::AppListShowSource> last_open_source_;

  base::ObserverList<ash::AppListControllerObserver> observers_;
};

}  // namespace test

#endif  // CHROME_BROWSER_ASH_APP_LIST_TEST_TEST_APP_LIST_CONTROLLER_H_
