// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_APP_SHORTCUTS_SEARCH_RESULT_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_APP_SHORTCUTS_SEARCH_RESULT_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/app_list/search/chrome_search_result.h"
#include "chrome/browser/ui/app_icon_loader_delegate.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "components/services/app_service/public/cpp/shortcut/shortcut.h"

class AppListControllerDelegate;
class AppServiceShortcutIconLoader;

namespace app_list {

// A search result for the AppShortcutsSearchProvider.
class AppShortcutSearchResult : public ChromeSearchResult,
                                public AppIconLoaderDelegate {
 public:
  AppShortcutSearchResult(const apps::ShortcutId& shortcut_id,
                          const std::u16string& title,
                          Profile* profile,
                          AppListControllerDelegate* app_list_controller,
                          double relevance);

  AppShortcutSearchResult(const AppShortcutSearchResult&) = delete;
  AppShortcutSearchResult& operator=(const AppShortcutSearchResult&) = delete;

  ~AppShortcutSearchResult() override;

  // ChromeSearchResult:
  void Open(int event_flags) override;

  // AppIconLoaderDelegate:
  void OnAppImageUpdated(
      const std::string& app_id,
      const gfx::ImageSkia& image,
      bool is_placeholder_icon,
      const std::optional<gfx::ImageSkia>& badge_image) override;

 private:
  const apps::ShortcutId shortcut_id_;
  const raw_ptr<Profile> profile_;  // Owned by ProfileInfo.
  const raw_ptr<AppListControllerDelegate> app_list_controller_;

  std::unique_ptr<AppServiceShortcutIconLoader> icon_loader_;

  base::WeakPtrFactory<AppShortcutSearchResult> weak_ptr_factory_{this};
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_APP_SHORTCUTS_SEARCH_RESULT_H_
