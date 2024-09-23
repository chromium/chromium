// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_OMNIBOX_OMNIBOX_LACROS_PROVIDER_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_OMNIBOX_OMNIBOX_LACROS_PROVIDER_H_

#include <optional>

#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/app_list/search/search_provider.h"
#include "chromeos/ash/components/string_matching/tokenized_string.h"
#include "chromeos/crosapi/mojom/launcher_search.mojom.h"
#include "components/omnibox/browser/autocomplete_input.h"

class AppListControllerDelegate;
class Profile;

namespace crosapi {
class SearchControllerAsh;
class SearchProviderAsh;
}  // namespace crosapi

namespace app_list {

// `OmniboxLacrosProvider` wraps `crosapi::SearchControllerAsh` - an ash-chrome
// interface to a lacros-chrome `AutocompleteController` - to provide omnibox
// results.
class OmniboxLacrosProvider : public SearchProvider {
 public:
  using SearchControllerCallback =
      base::RepeatingCallback<crosapi::SearchControllerAsh*()>;

  OmniboxLacrosProvider(Profile* profile,
                        AppListControllerDelegate* list_controller,
                        SearchControllerCallback search_controller_callback);
  ~OmniboxLacrosProvider() override;

  // SearchProvider:
  void Start(const std::u16string& query) override;
  void StopQuery() override;
  ash::AppListSearchResultType ResultType() const override;

  // Returns a `SearchControllerCallback` which returns a shared
  // `crosapi::SearchControllerAsh` from the singleton
  // `crosapi::SearchProviderAsh`.
  // As the controller may be shared among other `OmniboxLacrosProvider`
  // instances, calling `Start` will interrupt any unfinished searches in other
  // instances which also use `crosapi::SearchProviderAsh`'s controller.
  static SearchControllerCallback GetSingletonControllerCallback();

 private:
  void OnResultsReceived(std::vector<crosapi::mojom::SearchResultPtr> results);
  void StartWithoutSearchProvider(const std::u16string& query);

  SearchControllerCallback search_controller_callback_;
  const raw_ptr<Profile> profile_;
  const raw_ptr<AppListControllerDelegate> list_controller_;

  AutocompleteInput input_;

  std::u16string last_query_;
  std::optional<ash::string_matching::TokenizedString> last_tokenized_query_;

  // The AutocompleteController can sometimes update its results more than once
  // after reporting it is done. This flag is set to ensure we only update the
  // UI once.
  bool query_finished_ = false;

  base::WeakPtrFactory<OmniboxLacrosProvider> weak_factory_{this};
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_OMNIBOX_OMNIBOX_LACROS_PROVIDER_H_
