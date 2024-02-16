// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_OMNIBOX_OMNIBOX_PROVIDER_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_OMNIBOX_OMNIBOX_PROVIDER_H_

#include <algorithm>
#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ash/app_list/search/search_provider.h"
#include "chromeos/ash/components/string_matching/tokenized_string.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/favicon_cache.h"

class AppListControllerDelegate;
class AutocompleteController;
class AutocompleteResult;
class Profile;

namespace app_list {

// OmniboxProvider wraps AutocompleteController to provide omnibox results.
class OmniboxProvider : public SearchProvider,
                        public AutocompleteController::Observer {
 public:
  // `provider_types` is a bitmap containing AutocompleteProvider::Type values
  explicit OmniboxProvider(Profile* profile,
                           AppListControllerDelegate* list_controller,
                           int provider_types);

  OmniboxProvider(const OmniboxProvider&) = delete;
  OmniboxProvider& operator=(const OmniboxProvider&) = delete;

  ~OmniboxProvider() override;

  // SearchProvider overrides:
  void Start(const std::u16string& query) override;
  void StopQuery() override;
  ash::AppListSearchResultType ResultType() const override;

  // Populates result list from AutocompleteResult.
  void PopulateFromACResult(const AutocompleteResult& result);

  // Change the controller_ for testing purpose.
  void set_controller_for_test(
      std::unique_ptr<AutocompleteController> controller) {
    controller_ = std::move(controller);
  }

 private:
  // AutocompleteController::Observer overrides:
  void OnResultChanged(AutocompleteController* controller,
                       bool default_match_changed) override;

  raw_ptr<Profile> profile_;
  raw_ptr<AppListControllerDelegate> list_controller_;

  std::u16string last_query_;
  std::optional<ash::string_matching::TokenizedString> last_tokenized_query_;
  base::TimeTicks query_start_time_;
  AutocompleteInput input_;

  // The omnibox AutocompleteController that collects/sorts/dup-
  // eliminates the results as they come in.
  std::unique_ptr<AutocompleteController> controller_;

  // The AutocompleteController can sometimes update its results more than once
  // after reporting it is done. This flag is set to ensure we only update the
  // UI once.
  bool query_finished_ = false;

  FaviconCache favicon_cache_;
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_OMNIBOX_OMNIBOX_PROVIDER_H_
