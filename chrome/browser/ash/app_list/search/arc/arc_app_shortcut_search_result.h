// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_ARC_ARC_APP_SHORTCUT_SEARCH_RESULT_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_ARC_ARC_APP_SHORTCUT_SEARCH_RESULT_H_

#include <memory>
#include <string>

#include "ash/components/arc/mojom/app.mojom.h"
#include "ash/public/cpp/app_list/app_list_metrics.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/app_list/search/chrome_search_result.h"
#include "chrome/browser/ui/app_icon_loader_delegate.h"
#include "chromeos/ash/components/string_matching/tokenized_string.h"
#include "ui/gfx/image/image_skia.h"

class AppListControllerDelegate;
class AppServiceAppIconLoader;
class Profile;

namespace arc {
class IconDecodeRequest;
}  // namespace arc

namespace app_list {

namespace {
using ::ash::string_matching::TokenizedString;
}

class ArcAppShortcutSearchResult : public ChromeSearchResult,
                                   public AppIconLoaderDelegate {
 public:
  // Constructor for ArcAppShortcutSearchResult. `is_recommendation`
  // defines the display type of search results.
  ArcAppShortcutSearchResult(arc::mojom::AppShortcutItemPtr data,
                             Profile* profile,
                             AppListControllerDelegate* list_controller,
                             bool is_recommendation,
                             const TokenizedString& tokenized_query,
                             const std::string& details);

  ArcAppShortcutSearchResult(const ArcAppShortcutSearchResult&) = delete;
  ArcAppShortcutSearchResult& operator=(const ArcAppShortcutSearchResult&) =
      delete;

  ~ArcAppShortcutSearchResult() override;

  // ChromeSearchResult:
  void Open(int event_flags) override;

 private:
  // AppIconLoaderDelegate:
  void OnAppImageUpdated(
      const std::string& app_id,
      const gfx::ImageSkia& image,
      bool is_placeholder_icon,
      const std::optional<gfx::ImageSkia>& badge_image) override;

  // Gets app id of the app that publishes this app shortcut.
  std::string GetAppId() const;

  // Gets accessible name for this app shortcut.
  std::u16string ComputeAccessibleName() const;

  // Callback passed to |icon_decode_request_|.
  void OnIconDecoded(const gfx::ImageSkia&);

  arc::mojom::AppShortcutItemPtr data_;
  std::unique_ptr<arc::IconDecodeRequest> icon_decode_request_;

  std::unique_ptr<AppServiceAppIconLoader> badge_icon_loader_;

  const raw_ptr<Profile> profile_;  // Owned by ProfileInfo.
  const raw_ptr<AppListControllerDelegate>
      list_controller_;  // Owned by AppListClient.

  base::WeakPtrFactory<ArcAppShortcutSearchResult> weak_ptr_factory_{this};
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_ARC_ARC_APP_SHORTCUT_SEARCH_RESULT_H_
