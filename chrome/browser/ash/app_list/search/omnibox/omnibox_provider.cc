// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/omnibox/omnibox_provider.h"

#include <iterator>
#include <string>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/ash/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ash/app_list/search/omnibox/omnibox_answer_result.h"
#include "chrome/browser/ash/app_list/search/omnibox/omnibox_result.h"
#include "chrome/browser/ash/app_list/search/omnibox/omnibox_util.h"
#include "chrome/browser/ash/app_list/search/omnibox/open_tab_result.h"
#include "chrome/browser/ash/app_list/search/search_provider.h"
#include "chrome/browser/ash/app_list/search/types.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_provider_client.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_scheme_classifier.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/favicon/core/favicon_service.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/prefs/pref_service.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "third_party/metrics_proto/omnibox_focus_type.pb.h"
#include "third_party/omnibox_proto/answer_type.pb.h"
#include "url/gurl.h"

namespace app_list {

namespace {

using ::ash::string_matching::TokenizedString;

// Returns true if the match is an answer, including calculator answers.
bool IsAnswer(const AutocompleteMatch& match) {
  return match.answer_type != omnibox::ANSWER_TYPE_UNSPECIFIED ||
         match.type == AutocompleteMatchType::CALCULATOR;
}

}  //  namespace

// Control category is kept default intentionally as we always need to get
// answer cards results from Omnibox.
OmniboxProvider::OmniboxProvider(Profile* profile,
                                 AppListControllerDelegate* list_controller,
                                 int provider_types)
    : SearchProvider(SearchCategory::kOmnibox),
      profile_(profile),
      list_controller_(list_controller),
      favicon_cache_(FaviconServiceFactory::GetForProfile(
                         profile,
                         ServiceAccessType::EXPLICIT_ACCESS),
                     HistoryServiceFactory::GetForProfile(
                         profile,
                         ServiceAccessType::EXPLICIT_ACCESS)) {
  controller_ = std::make_unique<AutocompleteController>(
      std::make_unique<ChromeAutocompleteProviderClient>(profile),
      provider_types, /*is_cros_launcher=*/true),
  controller_->AddObserver(this);
}

OmniboxProvider::~OmniboxProvider() {}

void OmniboxProvider::Start(const std::u16string& query) {
  last_query_ = query;
  last_tokenized_query_.emplace(query, TokenizedString::Mode::kCamelCase);

  controller_->Stop(false);
  query_finished_ = false;
  // The new page classification value(CHROMEOS_APP_LIST) is introduced
  // to differentiate the suggest requests initiated by ChromeOS app_list from
  // the ones by Chrome omnibox.
  input_ =
      AutocompleteInput(query, metrics::OmniboxEventProto::CHROMEOS_APP_LIST,
                        ChromeAutocompleteSchemeClassifier(profile_));

  query_start_time_ = base::TimeTicks::Now();
  controller_->Start(input_);
}

void OmniboxProvider::StopQuery() {
  last_query_.clear();
  last_tokenized_query_.reset();
  query_finished_ = false;

  controller_->Stop(true);
}

ash::AppListSearchResultType OmniboxProvider::ResultType() const {
  return ash::AppListSearchResultType::kOmnibox;
}

void OmniboxProvider::PopulateFromACResult(const AutocompleteResult& result) {
  SearchProvider::Results new_results;
  new_results.reserve(result.size());

  std::vector<std::unique_ptr<OmniboxResult>> list_results;
  list_results.reserve(result.size());

  for (const AutocompleteMatch& match : result) {
    // Do not return a match in any of these cases:
    // - The URL is invalid.
    // - The URL points to Drive Web and is not an open tab. The Drive search
    //   provider surfaces Drive results.
    // - The URL points to a local file. The Local file search provider handles
    //   local file results, even if they've been opened in the browser.
    const bool is_drive = IsDriveUrl(match.destination_url) &&
                          match.type != AutocompleteMatchType::OPEN_TAB;
    if (!match.destination_url.is_valid() || is_drive ||
        match.destination_url.SchemeIsFile()) {
      continue;
    }

    if (match.type == AutocompleteMatchType::OPEN_TAB) {
      // Filters out open tab results if web in disabled in launcher search
      // controls.
      if (ash::features::IsLauncherSearchControlEnabled() &&
          !IsControlCategoryEnabled(profile_, ControlCategory::kWeb)) {
        continue;
      }
      DCHECK(last_tokenized_query_.has_value());
      new_results.emplace_back(std::make_unique<OpenTabResult>(
          profile_, list_controller_,
          CreateResult(match, controller_.get(), &favicon_cache_,
                       BookmarkModelFactory::GetForBrowserContext(profile_),
                       input_),
          last_tokenized_query_.value()));
    } else if (!IsAnswer(match)) {
      // Filters out omnibox results if web in disabled in launcher search
      // controls.
      if (ash::features::IsLauncherSearchControlEnabled() &&
          !IsControlCategoryEnabled(profile_, ControlCategory::kWeb)) {
        continue;
      }
      list_results.emplace_back(std::make_unique<OmniboxResult>(
          profile_, list_controller_,
          CreateResult(match, controller_.get(), &favicon_cache_,
                       BookmarkModelFactory::GetForBrowserContext(profile_),
                       input_),
          last_query_));
    } else {
      new_results.emplace_back(std::make_unique<OmniboxAnswerResult>(
          profile_, list_controller_,
          CreateAnswerResult(match, controller_.get(), last_query_, input_),
          last_query_));
    }
  }

  // Deduplicate the list results and then move-concatenate it into new_results.
  RemoveDuplicateResults(list_results);
  std::move(list_results.begin(), list_results.end(),
            std::back_inserter(new_results));

  if (controller_->done() && !query_finished_) {
    query_finished_ = true;
    SwapResults(&new_results);
  }
}

void OmniboxProvider::OnResultChanged(AutocompleteController* controller,
                                      bool default_match_changed) {
  DCHECK(controller == controller_.get());

  // Record the query latency.
  base::TimeDelta query_latency = base::TimeTicks::Now() - query_start_time_;
  UMA_HISTOGRAM_TIMES("Apps.AppList.OmniboxProvider.QueryTime", query_latency);

  PopulateFromACResult(controller_->result());
}

}  // namespace app_list
