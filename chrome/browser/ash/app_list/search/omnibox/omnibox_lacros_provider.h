// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_OMNIBOX_OMNIBOX_LACROS_PROVIDER_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_OMNIBOX_OMNIBOX_LACROS_PROVIDER_H_

#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/app_list/search/search_provider.h"
#include "chromeos/ash/components/string_matching/tokenized_string.h"
#include "chromeos/crosapi/mojom/launcher_search.mojom.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class AppListControllerDelegate;
class Profile;

namespace crosapi {
class CrosapiManager;
class SearchProviderAsh;
}  // namespace crosapi

namespace app_list {

class OmniboxLacrosProvider : public SearchProvider {
 public:
  OmniboxLacrosProvider(Profile* profile,
                        AppListControllerDelegate* list_controller,
                        crosapi::CrosapiManager* crosapi_manager);
  ~OmniboxLacrosProvider() override;

  // SearchProvider:
  void Start(const std::u16string& query) override;
  void StopQuery() override;
  ash::AppListSearchResultType ResultType() const override;

 private:
  void OnResultsReceived(std::vector<crosapi::mojom::SearchResultPtr> results);
  void StartWithoutSearchProvider(const std::u16string& query);

  raw_ptr<crosapi::SearchProviderAsh, ExperimentalAsh> search_provider_;
  const raw_ptr<Profile, ExperimentalAsh> profile_;
  const raw_ptr<AppListControllerDelegate, ExperimentalAsh> list_controller_;

  AutocompleteInput input_;

  std::u16string last_query_;
  absl::optional<ash::string_matching::TokenizedString> last_tokenized_query_;

  // The AutocompleteController can sometimes update its results more than once
  // after reporting it is done. This flag is set to ensure we only update the
  // UI once.
  bool query_finished_ = false;

  base::WeakPtrFactory<OmniboxLacrosProvider> weak_factory_{this};
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_OMNIBOX_OMNIBOX_LACROS_PROVIDER_H_
