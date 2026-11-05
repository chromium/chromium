// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_APP_RESULT_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_APP_RESULT_H_

#include <memory>
#include <string>

#include "ash/public/cpp/app_list/app_list_metrics.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/app_list/app_context_menu_delegate.h"
#include "chrome/browser/ash/app_list/search/chrome_search_result.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/icon_cache.h"
#include "components/services/app_service/public/cpp/icon_types.h"

class AppListControllerDelegate;
class Profile;

namespace base {
class Time;
}
namespace app_list {

class AppResult : public ChromeSearchResult, public AppContextMenuDelegate {
 public:
  AppResult(Profile* profile,
            const std::string& app_id,
            AppListControllerDelegate* controller,
            bool is_recommendation,
            apps::IconLoader* icon_loader);

  AppResult(const AppResult&) = delete;
  AppResult& operator=(const AppResult&) = delete;

  ~AppResult() override;

  void UpdateFromLastLaunchedOrInstalledTime(const base::Time& current_time,
                                             const base::Time& old_time);

  Profile* profile() const { return profile_; }

  const std::string& app_id() const { return app_id_; }

 private:
  // ChromeSearchResult overrides:
  void Open(int event_flags) override;

  // AppContextMenuDelegate overrides:
  void ExecuteLaunchCommand(int event_flags) override;

  ash::SearchResultType GetSearchResultType() const;
  void Launch(int event_flags, apps::LaunchSource launch_source);

  void CallLoadIcon(bool chip, bool allow_placeholder_icon);
  void OnLoadIcon(bool chip, apps::IconValuePtr icon_value);

  raw_ptr<Profile> profile_;
  const std::string app_id_;
  raw_ptr<AppListControllerDelegate> controller_;
  const raw_ptr<apps::IconLoader, DanglingUntriaged> icon_loader_;

  // When non-nullptr, signifies that this object is using the most recent icon
  // fetched from `icon_loader_`. When destroyed, informs `icon_loader_` that
  // the last icon is no longer used.
  std::unique_ptr<apps::IconLoader::Releaser> icon_loader_releaser_;

  apps::AppType app_type_;

  base::WeakPtrFactory<AppResult> weak_ptr_factory_{this};
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_APP_RESULT_H_
