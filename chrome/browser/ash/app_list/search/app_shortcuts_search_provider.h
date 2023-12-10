// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_APP_SHORTCUTS_SEARCH_PROVIDER_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_APP_SHORTCUTS_SEARCH_PROVIDER_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "chrome/browser/ash/app_list/search/search_provider.h"
#include "chrome/browser/profiles/profile.h"

namespace apps {
class ShortcutView;
}

namespace app_list {

class AppShortcutSearchResult;

// A new app shortcuts search provider.
class AppShortcutsSearchProvider : public SearchProvider {
 public:
  explicit AppShortcutsSearchProvider(Profile* profile);

  AppShortcutsSearchProvider(const AppShortcutsSearchProvider&) = delete;
  AppShortcutsSearchProvider& operator=(const AppShortcutsSearchProvider&) =
      delete;

  ~AppShortcutsSearchProvider() override;

  // SearchProvider overrides:
  void Start(const std::u16string& query) override;
  void StopQuery() override;
  ash::AppListSearchResultType ResultType() const override;

 private:
  void OnSearchComplete(const std::vector<apps::ShortcutView>& app_shortcuts);
  std::unique_ptr<AppShortcutSearchResult> MakeResult(
      const apps::ShortcutView& search_result,
      double relevance);

  base::TimeTicks query_start_time_;
  std::u16string last_query_;
  const raw_ptr<Profile, ExperimentalAsh> profile_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_APP_SHORTCUTS_SEARCH_PROVIDER_H_
