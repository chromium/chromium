// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_ARC_ARC_PLAYSTORE_SEARCH_RESULT_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_ARC_ARC_PLAYSTORE_SEARCH_RESULT_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "ash/components/arc/mojom/app.mojom.h"
#include "ash/public/cpp/app_list/app_list_metrics.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/app_list/app_context_menu_delegate.h"
#include "chrome/browser/ash/app_list/search/chrome_search_result.h"

class AppListControllerDelegate;

namespace arc {
class IconDecodeRequest;
}  // namespace arc

namespace app_list {

class ArcPlayStoreSearchResult : public ChromeSearchResult,
                                 public AppContextMenuDelegate {
 public:
  ArcPlayStoreSearchResult(arc::mojom::AppDiscoveryResultPtr data,
                           AppListControllerDelegate* list_controller,
                           const std::u16string& query);

  ArcPlayStoreSearchResult(const ArcPlayStoreSearchResult&) = delete;
  ArcPlayStoreSearchResult& operator=(const ArcPlayStoreSearchResult&) = delete;

  ~ArcPlayStoreSearchResult() override;

  // ChromeSearchResult overrides:
  void Open(int event_flags) override;

  // app_list::AppContextMenuDelegate overrides:
  void ExecuteLaunchCommand(int event_flags) override;

 private:
  const std::optional<std::string>& install_intent_uri() const {
    return data_->install_intent_uri;
  }
  const std::optional<std::string>& label() const { return data_->label; }
  bool is_instant_app() const { return data_->is_instant_app; }
  const std::optional<std::string>& formatted_price() const {
    return data_->formatted_price;
  }
  float review_score() const { return data_->review_score; }
  const std::vector<uint8_t>& icon_png_data() const {
    return data_->icon->icon_png_data.value();
  }

  // Callback passed to |icon_decode_request_|.
  void OnIconDecoded(const gfx::ImageSkia&);

  arc::mojom::AppDiscoveryResultPtr data_;
  std::unique_ptr<arc::IconDecodeRequest> icon_decode_request_;

  // |profile_| is owned by ProfileInfo.
  const raw_ptr<AppListControllerDelegate, DanglingUntriaged>
      list_controller_;  // Owned by AppListClient.

  base::WeakPtrFactory<ArcPlayStoreSearchResult> weak_ptr_factory_{this};
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_ARC_ARC_PLAYSTORE_SEARCH_RESULT_H_
