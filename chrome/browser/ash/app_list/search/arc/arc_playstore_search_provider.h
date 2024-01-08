// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_ARC_ARC_PLAYSTORE_SEARCH_PROVIDER_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_ARC_ARC_PLAYSTORE_SEARCH_PROVIDER_H_

#include <vector>

#include "ash/components/arc/mojom/app.mojom-forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/app_list/search/search_provider.h"

class Profile;
class AppListControllerDelegate;

namespace arc {
enum class ArcPlayStoreSearchRequestState;
}  // namespace arc

namespace base {
class TimeTicks;
}  // namespace base

namespace app_list {

class ArcPlayStoreSearchProvider : public SearchProvider {
 public:
  ArcPlayStoreSearchProvider(int max_results,
                             Profile* profile,
                             AppListControllerDelegate* list_controller);

  ArcPlayStoreSearchProvider(const ArcPlayStoreSearchProvider&) = delete;
  ArcPlayStoreSearchProvider& operator=(const ArcPlayStoreSearchProvider&) =
      delete;

  ~ArcPlayStoreSearchProvider() override;

  // SearchProvider:
  void Start(const std::u16string& query) override;
  void StopQuery() override;
  ash::AppListSearchResultType ResultType() const override;

 private:
  void OnResults(const std::u16string& query,
                 base::TimeTicks query_start_time,
                 arc::ArcPlayStoreSearchRequestState state,
                 std::vector<arc::mojom::AppDiscoveryResultPtr> results);

  const int max_results_;
  const raw_ptr<Profile> profile_;  // Owned by ProfileInfo.
  const raw_ptr<AppListControllerDelegate, DanglingUntriaged>
      list_controller_;        // Owned by AppListClient.
  std::u16string last_query_;  // Most recent query issued.
  base::WeakPtrFactory<ArcPlayStoreSearchProvider> weak_ptr_factory_{this};
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_ARC_ARC_PLAYSTORE_SEARCH_PROVIDER_H_
