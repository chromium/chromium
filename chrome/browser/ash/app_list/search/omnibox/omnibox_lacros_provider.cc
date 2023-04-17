// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/omnibox/omnibox_lacros_provider.h"

#include "chrome/browser/ash/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ash/app_list/search/omnibox/omnibox_answer_result.h"
#include "chrome/browser/ash/app_list/search/omnibox/omnibox_result.h"
#include "chrome/browser/ash/app_list/search/omnibox/omnibox_util.h"
#include "chrome/browser/ash/app_list/search/omnibox/open_tab_result.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/search_provider_ash.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_scheme_classifier.h"
#include "chrome/browser/chromeos/launcher_search/search_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chrome_web_ui_controller_factory.h"
#include "chromeos/ash/components/string_matching/tokenized_string.h"
#include "chromeos/crosapi/cpp/gurl_os_handler_utils.h"
#include "chromeos/crosapi/mojom/launcher_search.mojom.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/search_suggestion_parser.h"
#include "url/gurl.h"

namespace app_list {

// Note that there is necessarily a lot of overlap with code in the non-lacros
// omnibox provider, since this is implementing the same behavior (but using
// crosapi types).

namespace {

using ::ash::string_matching::TokenizedString;
using CrosApiSearchResult = ::crosapi::mojom::SearchResult;

}  // namespace

OmniboxLacrosProvider::OmniboxLacrosProvider(
    Profile* profile,
    AppListControllerDelegate* list_controller,
    crosapi::CrosapiManager* crosapi_manager)
    : search_provider_(nullptr),
      profile_(profile),
      list_controller_(list_controller) {
  DCHECK(profile_);
  DCHECK(list_controller_);

  if (crosapi_manager) {
    search_provider_ = crosapi_manager->crosapi_ash()->search_provider_ash();
    DCHECK(search_provider_);
  }
}

OmniboxLacrosProvider::~OmniboxLacrosProvider() = default;

void OmniboxLacrosProvider::Start(const std::u16string& query) {
  if (!search_provider_ || !search_provider_->IsSearchControllerConnected()) {
    const bool is_system_url =
        ChromeWebUIControllerFactory::GetInstance()->CanHandleUrl(GURL(query));
    if (is_system_url) {
      AutocompleteInput input;

      SearchSuggestionParser::SuggestResult suggest_result(
          query, AutocompleteMatchType::URL_WHAT_YOU_TYPED, /*subtypes=*/{},
          /*from_keyword=*/false,
          /*relevance=*/kMaxOmniboxScore, /*relevance_from_server=*/false,
          /*input_text=*/query);
      AutocompleteMatch match(/*provider=*/nullptr, suggest_result.relevance(),
                              /*deletable=*/false, suggest_result.type());
      match.destination_url = GURL(query);
      match.allowed_to_be_default_match = true;
      match.contents = suggest_result.match_contents();
      match.contents_class = suggest_result.match_contents_class();
      match.suggestion_group_id = suggest_result.suggestion_group_id();
      match.answer = suggest_result.answer();
      match.stripped_destination_url = GURL(query);

      crosapi::mojom::SearchResultPtr result =
          crosapi::CreateResult(match, /*controller=*/nullptr,
                                /*favicon_cache=*/nullptr,
                                /*bookmark_model=*/nullptr, input);

      SearchProvider::Results new_results;
      new_results.emplace_back(std::make_unique<OmniboxResult>(
          profile_, list_controller_, std::move(result), query));
      SwapResults(&new_results);
    }
    return;
  }

  last_query_ = query;
  last_tokenized_query_.emplace(query, TokenizedString::Mode::kCamelCase);

  query_finished_ = false;
  // Use page classification value CHROMEOS_APP_LIST to differentiate the
  // suggest requests initiated by ChromeOS app_list from the ones by Chrome
  // omnibox.
  input_ =
      AutocompleteInput(query, metrics::OmniboxEventProto::CHROMEOS_APP_LIST,
                        ChromeAutocompleteSchemeClassifier(profile_));

  search_provider_->Search(
      query, base::BindRepeating(&OmniboxLacrosProvider::OnResultsReceived,
                                 weak_factory_.GetWeakPtr()));
}

void OmniboxLacrosProvider::StopQuery() {
  last_query_.clear();
  last_tokenized_query_.reset();
  query_finished_ = false;
  weak_factory_.InvalidateWeakPtrs();
}

ash::AppListSearchResultType OmniboxLacrosProvider::ResultType() const {
  return ash::AppListSearchResultType::kOmnibox;
}

void OmniboxLacrosProvider::OnResultsReceived(
    std::vector<crosapi::mojom::SearchResultPtr> results) {
  SearchProvider::Results new_results;
  new_results.reserve(results.size());

  std::vector<std::unique_ptr<OmniboxResult>> list_results;
  list_results.reserve(results.size());

  for (auto&& search_result : results) {
    // Do not return a match in any of these cases:
    // - The URL is invalid.
    // - The URL points to Drive Web and is not an open tab. The Drive search
    //   provider surfaces Drive results.
    // - The URL points to a local file. The Local file search provider handles
    //   local file results, even if they've been opened in the browser.
    const GURL& url = *search_result->destination_url;
    const bool is_drive =
        IsDriveUrl(url) && search_result->omnibox_type !=
                               CrosApiSearchResult::OmniboxType::kOpenTab;
    if (!url.is_valid() || is_drive || url.SchemeIsFile())
      continue;

    if (search_result->omnibox_type ==
        CrosApiSearchResult::OmniboxType::kOpenTab) {
      // Open tab result.
      DCHECK(last_tokenized_query_.has_value());
      new_results.emplace_back(std::make_unique<OpenTabResult>(
          profile_, list_controller_, std::move(search_result),
          last_tokenized_query_.value()));
    } else if (!crosapi::OptionalBoolIsTrue(search_result->is_answer)) {
      // Omnibox result.
      list_results.emplace_back(std::make_unique<OmniboxResult>(
          profile_, list_controller_, std::move(search_result), last_query_));
    } else {
      // Answer result.
      new_results.emplace_back(std::make_unique<OmniboxAnswerResult>(
          profile_, list_controller_, std::move(search_result), last_query_));
    }
  }

  // Deduplicate the list results and then move-concatenate it into new_results.
  RemoveDuplicateResults(list_results);
  std::move(list_results.begin(), list_results.end(),
            std::back_inserter(new_results));

  // The search system requires only return once per StartSearch, so we need to
  // ensure no further results swap after the first one.
  if (!query_finished_) {
    query_finished_ = true;
    SwapResults(&new_results);
  }
}

}  // namespace app_list
