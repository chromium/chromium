// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_APP_SERVICE_APP_RESULT_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_APP_SERVICE_APP_RESULT_H_

#include <memory>

#include "ash/public/cpp/app_list/app_list_metrics.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "chrome/browser/ash/app_list/search/app_result.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/icon_cache.h"
#include "components/services/app_service/public/cpp/icon_types.h"

class AppListControllerDelegate;
class Profile;
namespace app_list {

class AppServiceAppResult : public AppResult {
 public:
  AppServiceAppResult(Profile* profile,
                      const std::string& app_id,
                      AppListControllerDelegate* controller,
                      bool is_recommendation,
                      apps::IconLoader* icon_loader);

  AppServiceAppResult(const AppServiceAppResult&) = delete;
  AppServiceAppResult& operator=(const AppServiceAppResult&) = delete;

  ~AppServiceAppResult() override;

 private:
  // ChromeSearchResult overrides:
  void Open(int event_flags) override;

  // AppContextMenuDelegate overrides:
  void ExecuteLaunchCommand(int event_flags) override;

  ash::SearchResultType GetSearchResultType() const;
  void Launch(int event_flags, apps::LaunchSource launch_source);

  void CallLoadIcon(bool chip, bool allow_placeholder_icon);
  void OnLoadIcon(bool chip, apps::IconValuePtr icon_value);

  const raw_ptr<apps::IconLoader, DanglingUntriaged> icon_loader_;

  // When non-nullptr, signifies that this object is using the most recent icon
  // fetched from |icon_loader_|. When destroyed, informs |icon_loader_| that
  // the last icon is no longer used.
  std::unique_ptr<apps::IconLoader::Releaser> icon_loader_releaser_;

  apps::AppType app_type_;
  bool is_platform_app_;
  bool show_in_launcher_;

  base::CancelableTaskTracker task_tracker_;

  base::WeakPtrFactory<AppServiceAppResult> weak_ptr_factory_{this};
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_APP_SERVICE_APP_RESULT_H_
