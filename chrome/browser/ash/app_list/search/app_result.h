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

class AppListControllerDelegate;
class Profile;

namespace base {
class Time;
}
namespace app_list {

class AppResult : public ChromeSearchResult, public AppContextMenuDelegate {
 public:
  AppResult(const AppResult&) = delete;
  AppResult& operator=(const AppResult&) = delete;

  ~AppResult() override;

  void UpdateFromLastLaunchedOrInstalledTime(const base::Time& current_time,
                                             const base::Time& old_time);

  // Marked const in order to be able to use in derived class in const methods.
  Profile* profile() const { return profile_; }

  const std::string& app_id() const { return app_id_; }

  base::WeakPtr<AppResult> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 protected:
  AppResult(Profile* profile,
            const std::string& app_id,
            AppListControllerDelegate* controller,
            bool is_recommendation);

  // Marked const in order to be able to use in derived class in const methods.
  AppListControllerDelegate* controller() const {
    return controller_;
  }

 private:
  raw_ptr<Profile> profile_;
  const std::string app_id_;
  raw_ptr<AppListControllerDelegate> controller_;

  base::WeakPtrFactory<AppResult> weak_ptr_factory_{this};
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_APP_RESULT_H_
