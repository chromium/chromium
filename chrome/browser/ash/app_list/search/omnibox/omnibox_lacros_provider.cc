// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/omnibox/omnibox_lacros_provider.h"

#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/functional/bind.h"
#include "chrome/browser/ash/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ash/app_list/search/omnibox/omnibox_answer_result.h"
#include "chrome/browser/ash/app_list/search/omnibox/omnibox_result.h"
#include "chrome/browser/ash/app_list/search/omnibox/omnibox_util.h"
#include "chrome/browser/ash/app_list/search/omnibox/open_tab_result.h"
#include "chrome/browser/ash/app_list/search/types.h"
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
#include "components/prefs/pref_service.h"
#include "third_party/omnibox_proto/navigational_intent.pb.h"
#include "url/gurl.h"

namespace app_list {

// Note that there is necessarily a lot of overlap with code in the non-lacros
// omnibox provider, since this is implementing the same behavior (but using
// crosapi types).

namespace {

using ::ash::string_matching::TokenizedString;
using CrosApiSearchResult = ::crosapi::mojom::SearchResult;

}  // namespace

// Control category is kept default intentionally as we always need to get
// answer cards results from Omnibox.
OmniboxLacrosProvider::OmniboxLacrosProvider(
    Profile* profile,
    AppListControllerDelegate* list_controller,
    SearchControllerCallback search_controller_callback)
    : SearchProvider(SearchCategory::kOmnibox),
      search_controller_callback_(std::move(search_controller_callback)),
      profile_(profile),
      list_controller_(list_controller) {
  DCHECK(profile_);
  DCHECK(list_controller_);
}

OmniboxLacrosProvider::~OmniboxLacrosProvider() = default;

// static
OmniboxLacrosProvider::SearchControllerCallback
OmniboxLacrosProvider::GetSingletonControllerCallback() {
  return base::BindRepeating([]() -> crosapi::SearchControllerAsh* {
    crosapi::SearchProviderAsh* search_provider =
        crosapi::CrosapiManager::Get()->crosapi_ash()->search_provider_ash();
    if (!search_provider) {
      return nullptr;
    }
    return search_provider->GetController();
  });
}

void OmniboxLacrosProvider::StartWithoutSearchProvider(
    const std::u16string& query) {
  // If Lacros is unexpectedly not available (e.g. mount failure), make sure
  // that at least known system (Ash) URLs can be found. AppListClient will
  // handle these directly (without involving Lacros), so that one can still
  // open tools such as os://flags.
  GURL url(query);
  if (crosapi::gurl_os_handler_utils::HasOsScheme(url) &&
      ChromeWebUIControllerFactory::GetInstance()->CanHandleUrl(
          crosapi::gurl_os_handler_utils::GetAshUrlFromLacrosUrl(url))) {
    AutocompleteInput input;

    SearchSuggestionParser::SuggestResult suggest_result(
        query, AutocompleteMatchType::URL_WHAT_YOU_TYPED,
        /*suggest_type=*/omnibox::TYPE_NATIVE_CHROME, /*subtypes=*/{},
        /*from_keyword=*/false,
        /*navigational_intent=*/omnibox::NAV_INTENT_NONE,
        /*relevance=*/kMaxOmniboxScore, /*relevance_from_server=*/false,
        /*input_text=*/query);
    AutocompleteMatch match(/*provider=*/nullptr, suggest_result.relevance(),
                            /*deletable=*/false, suggest_result.type());
    match.destination_url = url;
    match.allowed_to_be_default_match = true;
    match.contents = suggest_result.match_contents();
    match.contents_class = suggest_result.match_contents_class();
    match.suggestion_group_id = suggest_result.suggestion_group_id();
    match.answer = suggest_result.answer();
    match.answer_template = suggest_result.answer_template();
    match.answer_type = suggest_result.answer_type();
    match.stripped_destination_url = url;

    crosapi::mojom::SearchResultPtr result =
        crosapi::CreateResult(match, /*controller=*/nullptr,
                              /*favicon_cache=*/nullptr,
                              /*bookmark_model=*/nullptr, input);

    SearchProvider::Results new_results;
    new_results.emplace_back(std::make_unique<OmniboxResult>(
        profile_, list_controller_, std::move(result), query));
    SwapResults(&new_results);
  }
}

void OmniboxLacrosProvider::Start(const std::u16string& query) {
  crosapi::SearchControllerAsh* search_controller =
      search_controller_callback_.Run();
  if (!search_controller || !search_controller->IsConnected()) {
    StartWithoutSearchProvider(query);
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

  search_controller->Search(
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
      // Filters out open tab results if web in disabled in launcher search
      // controls.
      if (ash::features::IsLauncherSearchControlEnabled() &&
          !IsControlCategoryEnabled(profile_, ControlCategory::kWeb)) {
        continue;
      }
      // Open tab result.
      DCHECK(last_tokenized_query_.has_value());
      new_results.emplace_back(std::make_unique<OpenTabResult>(
          profile_, list_controller_, std::move(search_result),
          last_tokenized_query_.value()));
    } else if (!crosapi::OptionalBoolIsTrue(search_result->is_answer)) {
      // Filters out omnibox results if web in disabled in launcher search
      // controls.
      if (ash::features::IsLauncherSearchControlEnabled() &&
          !IsControlCategoryEnabled(profile_, ControlCategory::kWeb)) {
        continue;
      }
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
