// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_APP_SHORTCUTS_SEARCH_RESULT_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_APP_SHORTCUTS_SEARCH_RESULT_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/app_list/search/chrome_search_result.h"

namespace app_list {

// A search result for the AppShortcutsSearchProvider.
class AppShortcutSearchResult : public ChromeSearchResult {
 public:
  AppShortcutSearchResult(const std::string& id,
                          const std::u16string& title,
                          Profile* profile,
                          double relevance);

  AppShortcutSearchResult(const AppShortcutSearchResult&) = delete;
  AppShortcutSearchResult& operator=(const AppShortcutSearchResult&) = delete;

  ~AppShortcutSearchResult() override;

  // ChromeSearchResult:
  void Open(int event_flags) override;

 private:
  const raw_ptr<Profile, ExperimentalAsh> profile_;  // Owned by ProfileInfo.
};
}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_APP_SHORTCUTS_SEARCH_RESULT_H_
