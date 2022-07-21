// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefetch/search_prefetch/search_prefetch_service.h"

#include <iterator>
#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/json/values_util.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/values.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/prefetch/pref_names.h"
#include "chrome/browser/prefetch/prefetch_prefs.h"
#include "chrome/browser/prefetch/search_prefetch/cache_alias_search_prefetch_url_loader.h"
#include "chrome/browser/prefetch/search_prefetch/field_trial_settings.h"
#include "chrome/browser/prefetch/search_prefetch/search_prefetch_url_loader.h"
#include "chrome/browser/preloading/prerender/prerender_manager.h"
#include "chrome/browser/preloading/prerender/prerender_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/search_engines/ui_thread_search_terms_data.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/omnibox/browser/base_search_provider.h"
#include "components/omnibox/browser/omnibox_event_global_tracker.h"
#include "components/omnibox/browser/omnibox_log.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/web_contents.h"
#include "net/base/load_flags.h"
#include "net/base/url_util.h"
#include "services/network/public/cpp/resource_request.h"
#include "ui/base/page_transition_types.h"
#include "url/origin.h"

namespace {

// Recomputes the destination URL for |match| with the updated prefetch
// information (does not modify |destination_url|). Passing true to
// |attach_prefetch_information| if the URL request will be sent to network,
// otherwise set to false if it is for client-internal use only.
GURL GetPreloadURLFromMatch(const AutocompleteMatch& match,
                            TemplateURLService* template_url_service,
                            bool attach_prefetch_information) {
  // Copy the search term args, so we can modify them for just the prefetch.
  auto search_terms_args = *(match.search_terms_args);
  search_terms_args.is_prefetch = attach_prefetch_information;
  return GURL(template_url_service->GetDefaultSearchProvider()
                  ->url_ref()
                  .ReplaceSearchTerms(search_terms_args,
                                      template_url_service->search_terms_data(),
                                      nullptr));
}

struct SearchPrefetchEligibilityReasonRecorder {
 public:
  explicit SearchPrefetchEligibilityReasonRecorder(bool navigation_prefetch)
      : navigation_prefetch_(navigation_prefetch) {}
  ~SearchPrefetchEligibilityReasonRecorder() {
    if (navigation_prefetch_) {
      UMA_HISTOGRAM_ENUMERATION(
          "Omnibox.SearchPrefetch.PrefetchEligibilityReason.NavigationPrefetch",
          reason_);
    } else {
      UMA_HISTOGRAM_ENUMERATION(
          "Omnibox.SearchPrefetch.PrefetchEligibilityReason.SuggestionPrefetch",
          reason_);
    }
  }

  SearchPrefetchEligibilityReason reason_ =
      SearchPrefetchEligibilityReason::kPrefetchStarted;
  bool navigation_prefetch_;
};

void RecordFinalStatus(SearchPrefetchStatus status, bool navigation_prefetch) {
  if (navigation_prefetch) {
    UMA_HISTOGRAM_ENUMERATION(
        "Omnibox.SearchPrefetch.PrefetchFinalStatus.NavigationPrefetch",
        status);
  } else {
    UMA_HISTOGRAM_ENUMERATION(
        "Omnibox.SearchPrefetch.PrefetchFinalStatus.SuggestionPrefetch",
        status);
  }
}

bool ShouldPrefetch(const AutocompleteMatch& match) {
  // Prerender's threshold should definitely be higher than prefetch's. So a
  // prerender hints can be treated as a prefetch hint.
  return BaseSearchProvider::ShouldPrefetch(match) ||
         BaseSearchProvider::ShouldPrerender(match);
}

}  // namespace

struct SearchPrefetchService::SearchPrefetchServingReasonRecorder {
 public:
  explicit SearchPrefetchServingReasonRecorder(bool for_prerender)
      : for_prerender_(for_prerender) {}
  ~SearchPrefetchServingReasonRecorder() {
    base::UmaHistogramEnumeration(
        for_prerender_
            ? "Omnibox.SearchPrefetch.PrefetchServingReason.Prerender"
            : "Omnibox.SearchPrefetch.PrefetchServingReason",
        reason_);
  }

  SearchPrefetchServingReason reason_ = SearchPrefetchServingReason::kServed;
  const bool for_prerender_ = false;
};

// static
void SearchPrefetchService::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  // Some loss in this pref (especially following a browser crash) is well
  // tolerated and helps ensure the pref service isn't slammed.
  registry->RegisterDictionaryPref(prefetch::prefs::kCachePrefPath,
                                   PrefRegistry::LOSSY_PREF);
}

SearchPrefetchService::SearchPrefetchService(Profile* profile)
    : profile_(profile) {
  DCHECK(!profile_->IsOffTheRecord());

  if (LoadFromPrefs())
    SaveToPrefs();
}

SearchPrefetchService::~SearchPrefetchService() = default;

void SearchPrefetchService::Shutdown() {
  observer_.Reset();
}

bool SearchPrefetchService::MaybePrefetchURL(const GURL& url) {
  return MaybePrefetchURL(url, /*navigation_prefetch=*/false);
}

bool SearchPrefetchService::MaybePrefetchURL(const GURL& url,
                                             bool navigation_prefetch) {
  if (!SearchPrefetchServicePrefetchingIsEnabled())
    return false;

  SearchPrefetchEligibilityReasonRecorder recorder(navigation_prefetch);

  if (!prefetch::IsSomePreloadingEnabled(*profile_->GetPrefs())) {
    recorder.reason_ = SearchPrefetchEligibilityReason::kPrefetchDisabled;
    return false;
  }

  if (!profile_->GetPrefs() ||
      !profile_->GetPrefs()->GetBoolean(prefs::kWebKitJavascriptEnabled)) {
    recorder.reason_ = SearchPrefetchEligibilityReason::kJavascriptDisabled;
    return false;
  }

  auto* content_settings =
      HostContentSettingsMapFactory::GetForProfile(profile_);
  if (!content_settings ||
      content_settings->GetContentSetting(
          url, url, ContentSettingsType::JAVASCRIPT) == CONTENT_SETTING_BLOCK) {
    recorder.reason_ = SearchPrefetchEligibilityReason::kJavascriptDisabled;
    return false;
  }

  auto* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile_);
  if (!template_url_service ||
      !template_url_service->GetDefaultSearchProvider()) {
    recorder.reason_ = SearchPrefetchEligibilityReason::kSearchEngineNotValid;
    return false;
  }

  // Lazily observe Template URL Service.
  ObserveTemplateURLService(template_url_service);

  std::u16string search_terms;

  // Extract the terms directly to make sure this string will match the URL
  // interception string logic.
  template_url_service->GetDefaultSearchProvider()->ExtractSearchTermsFromURL(
      url, template_url_service->search_terms_data(), &search_terms);

  if (search_terms.size() == 0) {
    recorder.reason_ =
        SearchPrefetchEligibilityReason::kNotDefaultSearchWithTerms;
    return false;
  }

  if (!navigation_prefetch &&
      (last_error_time_ticks_ + SearchPrefetchErrorBackoffDuration() >
       base::TimeTicks::Now())) {
    recorder.reason_ = SearchPrefetchEligibilityReason::kErrorBackoff;
    return false;
  }

  // Don't prefetch the same search terms twice within the expiry duration.
  if (prefetches_.find(search_terms) != prefetches_.end()) {
    auto status = prefetches_[search_terms]->current_status();

    // If the prefetch is for navigation it can replace unservable statuses.
    if (!navigation_prefetch || status == SearchPrefetchStatus::kCanBeServed ||
        status == SearchPrefetchStatus::kCanBeServedAndUserClicked ||
        status == SearchPrefetchStatus::kComplete ||
        status == SearchPrefetchStatus::kPrerendered) {
      recorder.reason_ =
          SearchPrefetchEligibilityReason::kAttemptedQueryRecently;
      return false;
    }

    // The navigation prefetch should replace the existing prefetch.
    if (navigation_prefetch)
      DeletePrefetch(search_terms);
  }

  if (prefetches_.size() >= SearchPrefetchMaxAttemptsPerCachingDuration()) {
    recorder.reason_ = SearchPrefetchEligibilityReason::kMaxAttemptsReached;
    return false;
  }

  std::unique_ptr<SearchPrefetchRequest> prefetch_request =
      std::make_unique<SearchPrefetchRequest>(
          search_terms, url, navigation_prefetch,
          base::BindOnce(&SearchPrefetchService::ReportFetchResult,
                         base::Unretained(this)));

  DCHECK(prefetch_request);
  if (!prefetch_request->StartPrefetchRequest(profile_)) {
    recorder.reason_ = SearchPrefetchEligibilityReason::kThrottled;
    return false;
  }

  prefetches_.emplace(search_terms, std::move(prefetch_request));
  prefetch_expiry_timers_.emplace(search_terms,
                                  std::make_unique<base::OneShotTimer>());
  prefetch_expiry_timers_[search_terms]->Start(
      FROM_HERE, SearchPrefetchCachingLimit(),
      base::BindOnce(&SearchPrefetchService::DeletePrefetch,
                     base::Unretained(this), search_terms));
  return true;
}

void SearchPrefetchService::OnURLOpenedFromOmnibox(OmniboxLog* log) {
  if (!log)
    return;
  const GURL& opened_url = log->final_destination_url;

  auto& match = log->result.match_at(log->selected_index);
  if (match.type == AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED) {
    bool has_search_suggest = false;
    bool has_history_search = false;
    for (auto& duplicate_match : match.duplicate_matches) {
      if (duplicate_match.type == AutocompleteMatchType::SEARCH_SUGGEST ||
          AutocompleteMatch::IsSpecializedSearchType(duplicate_match.type)) {
        has_search_suggest = true;
      }
      if (duplicate_match.type == AutocompleteMatchType::SEARCH_HISTORY) {
        has_history_search = true;
      }
    }

    base::UmaHistogramBoolean(
        "Omnibox.SearchPrefetch.SearchWhatYouTypedWasAlsoSuggested.Suggest",
        has_search_suggest);
    base::UmaHistogramBoolean(
        "Omnibox.SearchPrefetch.SearchWhatYouTypedWasAlsoSuggested.History",
        has_history_search);
    base::UmaHistogramBoolean(
        "Omnibox.SearchPrefetch.SearchWhatYouTypedWasAlsoSuggested."
        "HistoryOrSuggest",
        has_history_search || has_search_suggest);
  }

  auto* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile_);
  DCHECK(template_url_service);
  auto* default_search = template_url_service->GetDefaultSearchProvider();
  if (!default_search)
    return;

  std::u16string match_search_terms;

  default_search->ExtractSearchTermsFromURL(
      opened_url, template_url_service->search_terms_data(),
      &match_search_terms);

  if (match_search_terms.size() == 0)
    return;

  if (prefetches_.find(match_search_terms) == prefetches_.end()) {
    return;
  }
  SearchPrefetchRequest& prefetch = *prefetches_[match_search_terms];
  prefetch.RecordClickTime();

  if (prefetch.current_status() != SearchPrefetchStatus::kCanBeServed &&
      prefetch.current_status() != SearchPrefetchStatus::kPrerendered) {
    return;
  }
  prefetch.MarkPrefetchAsClicked();
}

void SearchPrefetchService::AddCacheEntryForPrerender(
    const GURL& updated_prerendered_url,
    const GURL& prerendering_url) {
  DCHECK(prerender_utils::IsSearchSuggestionPrerenderEnabled());

  // We do not need this method while running the search prefetch/prerender
  // unification experiment.
  DCHECK(!prerender_utils::SearchPrefetchUpgradeToPrerenderIsEnabled());
  AddCacheEntry(updated_prerendered_url, prerendering_url);
}

void SearchPrefetchService::OnPrerenderedRequestUsed(
    const std::u16string& search_terms,
    const GURL& navigation_url) {
  DCHECK(prerender_utils::SearchPrefetchUpgradeToPrerenderIsEnabled());

  auto request_it = prefetches_.find(search_terms);
  DCHECK(request_it != prefetches_.end());
  if (request_it == prefetches_.end()) {
    // TODO(https://crbug.com/1295170): It should be rare but the request can be
    // deleted by timer before chrome activates the page. Add some metrics to
    // understand the possibility.
    return;
  }
  AddCacheEntry(navigation_url, request_it->second->prefetch_url());
  request_it->second->MarkPrefetchAsPrerenderActivated();
  DeletePrefetch(search_terms);
}

std::unique_ptr<SearchPrefetchURLLoader>
SearchPrefetchService::TakePrerenderFromMemoryCache(
    const network::ResourceRequest& tentative_resource_request) {
  SearchPrefetchServingReasonRecorder recorder{/*for_prerender=*/true};
  auto iter =
      RetrieveSearchTermsInMemoryCache(tentative_resource_request, recorder);
  if (iter == prefetches_.end()) {
    return nullptr;
  }

  // TODO(https://crbug.com/1295170): Do not use the prefetched response if it
  // is about to expire.
  DCHECK_NE(iter->second->current_status(),
            SearchPrefetchStatus::kRequestFailed);
  recorder.reason_ = SearchPrefetchServingReason::kPrerendered;

  iter->second->MarkPrefetchAsPrerendered();
  std::unique_ptr<SearchPrefetchURLLoader> response =
      iter->second->TakeSearchPrefetchURLLoader();
  return response;
  // Do not remove the corresponding entry from `prefetches_` for now, to avoid
  // prefetching the same response over again. The entry will be removed on
  // prerendering activation or other cases.
}

absl::optional<SearchPrefetchStatus>
SearchPrefetchService::GetSearchPrefetchStatusForTesting(
    std::u16string search_terms) {
  if (prefetches_.find(search_terms) == prefetches_.end())
    return absl::nullopt;
  return prefetches_[search_terms]->current_status();
}

std::unique_ptr<SearchPrefetchURLLoader>
SearchPrefetchService::TakePrefetchResponseFromMemoryCache(
    const network::ResourceRequest& tentative_resource_request) {
  const GURL& navigation_url = tentative_resource_request.url;
  SearchPrefetchServingReasonRecorder recorder(/*for_prerender=*/false);

  auto iter =
      RetrieveSearchTermsInMemoryCache(tentative_resource_request, recorder);
  if (iter == prefetches_.end()) {
    DCHECK_NE(recorder.reason_, SearchPrefetchServingReason::kServed);
    return nullptr;
  }

  if (iter->second->current_status() != SearchPrefetchStatus::kComplete &&
      iter->second->current_status() !=
          SearchPrefetchStatus::kCanBeServedAndUserClicked) {
    recorder.reason_ = SearchPrefetchServingReason::kNotServedOtherReason;
    return nullptr;
  }

  std::unique_ptr<SearchPrefetchURLLoader> response =
      iter->second->TakeSearchPrefetchURLLoader();

  iter->second->MarkPrefetchAsServed();

  if (navigation_url != iter->second->prefetch_url())
    AddCacheEntry(navigation_url, iter->second->prefetch_url());

  DeletePrefetch(iter->first);

  return response;
}

std::unique_ptr<SearchPrefetchURLLoader>
SearchPrefetchService::TakePrefetchResponseFromDiskCache(
    const GURL& navigation_url) {
  GURL navigation_url_without_ref(net::SimplifyUrlForRequest(navigation_url));
  if (prefetch_cache_.find(navigation_url_without_ref) ==
      prefetch_cache_.end()) {
    return nullptr;
  }

  return std::make_unique<CacheAliasSearchPrefetchURLLoader>(
      profile_, SearchPrefetchRequest::NetworkAnnotationForPrefetch(),
      prefetch_cache_[navigation_url_without_ref].first);
}

void SearchPrefetchService::ClearPrefetches() {
  prefetches_.clear();
  prefetch_expiry_timers_.clear();
  prefetch_cache_.clear();
  SaveToPrefs();
}

void SearchPrefetchService::DeletePrefetch(std::u16string search_terms) {
  DCHECK(prefetches_.find(search_terms) != prefetches_.end());
  DCHECK(prefetch_expiry_timers_.find(search_terms) !=
         prefetch_expiry_timers_.end());

  RecordFinalStatus(prefetches_[search_terms]->current_status(),
                    prefetches_[search_terms]->navigation_prefetch());

  prefetches_.erase(search_terms);
  prefetch_expiry_timers_.erase(search_terms);
}

void SearchPrefetchService::ReportFetchResult(bool error) {
  UMA_HISTOGRAM_BOOLEAN("Omnibox.SearchPrefetch.FetchResult.SuggestionPrefetch",
                        !error);
  if (!error)
    return;
  last_error_time_ticks_ = base::TimeTicks::Now();
}

void SearchPrefetchService::OnResultChanged(content::WebContents* web_contents,
                                            const AutocompleteResult& result) {
  auto* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile_);
  DCHECK(template_url_service);
  auto* default_search = template_url_service->GetDefaultSearchProvider();
  if (!default_search)
    return;

  // Lazily observe Template URL Service.
  ObserveTemplateURLService(template_url_service);

  // Cancel Unneeded prefetch requests. Since we limit the number of prefetches
  // in the map, this should be fast despite the two loops.
  for (const auto& kv_pair : prefetches_) {
    const auto& search_terms = kv_pair.first;
    auto& prefetch_request = kv_pair.second;

    if (!prefetch_request->ShouldBeCancelledOnResultChanges()) {
      // Reset all pending prerenders. It will be set soon if service still
      // wants clients to prerender a SearchTerms.
      // TODO(https://crbug.com/1295170): Unlike prefetch, which does not
      // discard completed response to avoid wasting, prerender would like
      // to cancel itself given the cost of a prerender. For now prenderer is
      // canceled when the prerender hints changed, we need to revisit this
      // decision.
      prefetch_request->ResetPrerenderUpgrader();
      continue;
    }
    bool should_cancel_request = true;
    for (const auto& match : result) {
      std::u16string match_search_terms;
      default_search->ExtractSearchTermsFromURL(
          match.destination_url, template_url_service->search_terms_data(),
          &match_search_terms);

      if (search_terms == match_search_terms) {
        should_cancel_request = false;
        break;
      }
    }

    // Cancel the inflight request and mark it as canceled.
    if (should_cancel_request) {
      prefetch_request->CancelPrefetch();
    }

    // Reset all pending prerenders. It will be set soon if service still wants
    // clients to prerender a SearchTerms.
    prefetch_request->ResetPrerenderUpgrader();
  }

  // Do not perform preloading if there is no active tab.
  if (!web_contents)
    return;
  for (const auto& match : result) {
    if (prerender_utils::IsSearchSuggestionPrerenderEnabled() &&
        prerender_utils::SearchPrefetchUpgradeToPrerenderIsEnabled()) {
      if (!ShouldPrefetch(match))
        continue;
      CoordinatePrefetchWithPrerender(match, web_contents,
                                      template_url_service);
      continue;
    }

    if (BaseSearchProvider::ShouldPrefetch(match)) {
      MaybePrefetchURL(GetPreloadURLFromMatch(
          match, template_url_service, /*attach_prefetch_information=*/true));
    }
    if (prerender_utils::IsSearchSuggestionPrerenderEnabled() &&
        BaseSearchProvider::ShouldPrerender(match)) {
      PrerenderManager::CreateForWebContents(web_contents);
      auto* prerender_manager = PrerenderManager::FromWebContents(web_contents);
      DCHECK(prerender_manager);
      prerender_manager->StartPrerenderSearchSuggestion(match);
    }
  }
}

void SearchPrefetchService::MaybePrefetchLikelyMatch(
    size_t index,
    const AutocompleteMatch& match) {
  if (!IsSearchNavigationPrefetchEnabled())
    return;
  // Assume the user is going back to enter more for now.
  if (index == 0)
    return;
  // Only prefetch search types.
  if (!AutocompleteMatch::IsSearchType(match.type))
    return;
  // Check to make sure this is search related and that we can read the search
  // arguments. For Search history this may be null.
  if (!match.search_terms_args)
    return;
  auto* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile_);
  // The default search provider needs to opt into prefetching behavior.
  if (!template_url_service ||
      !template_url_service->GetDefaultSearchProvider() ||
      !template_url_service->GetDefaultSearchProvider()
           ->data()
           .prefetch_likely_navigations) {
    return;
  }
  MaybePrefetchURL(GetPreloadURLFromMatch(match, template_url_service,
                                          /*attach_prefetch_information=*/true),
                   /*navigation_prefetch=*/true);
}

void SearchPrefetchService::OnTemplateURLServiceChanged() {
  auto* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile_);
  DCHECK(template_url_service);

  absl::optional<TemplateURLData> template_url_service_data;

  const TemplateURL* template_url =
      template_url_service->GetDefaultSearchProvider();
  if (template_url) {
    template_url_service_data = template_url->data();
  }

  if (!template_url_service_data_.has_value() &&
      !template_url_service_data.has_value()) {
    return;
  }

  UIThreadSearchTermsData search_data;

  if (template_url_service_data_.has_value() &&
      template_url_service_data.has_value() &&
      TemplateURL::MatchesData(
          template_url, &(template_url_service_data_.value()), search_data)) {
    return;
  }

  template_url_service_data_ = template_url_service_data;
  ClearPrefetches();
}

void SearchPrefetchService::ClearCacheEntry(const GURL& navigation_url) {
  GURL navigation_url_without_ref(net::SimplifyUrlForRequest(navigation_url));
  if (prefetch_cache_.find(navigation_url_without_ref) ==
      prefetch_cache_.end()) {
    return;
  }

  prefetch_cache_.erase(navigation_url_without_ref);
  SaveToPrefs();
}

void SearchPrefetchService::UpdateServeTime(const GURL& navigation_url) {
  GURL navigation_url_without_ref(net::SimplifyUrlForRequest(navigation_url));
  if (prefetch_cache_.find(navigation_url_without_ref) == prefetch_cache_.end())
    return;

  prefetch_cache_[navigation_url_without_ref].second = base::Time::Now();
  SaveToPrefs();
}

void SearchPrefetchService::AddCacheEntry(const GURL& navigation_url,
                                          const GURL& prefetch_url) {
  GURL navigation_url_without_ref(net::SimplifyUrlForRequest(navigation_url));
  GURL prefetch_url_without_ref(net::SimplifyUrlForRequest(prefetch_url));
  if (navigation_url_without_ref == prefetch_url_without_ref) {
    return;
  }

  prefetch_cache_.emplace(
      navigation_url_without_ref,
      std::make_pair(prefetch_url_without_ref, base::Time::Now()));

  if (prefetch_cache_.size() <= SearchPrefetchMaxCacheEntries()) {
    SaveToPrefs();
    return;
  }

  GURL url_to_remove;
  base::Time earliest_time = base::Time::Now();
  for (const auto& entry : prefetch_cache_) {
    base::Time last_used_time = entry.second.second;
    if (last_used_time < earliest_time) {
      earliest_time = last_used_time;
      url_to_remove = entry.first;
    }
  }
  ClearCacheEntry(url_to_remove);
  SaveToPrefs();
}

bool SearchPrefetchService::LoadFromPrefs() {
  prefetch_cache_.clear();
  const base::Value::Dict& dictionary =
      profile_->GetPrefs()->GetValueDict(prefetch::prefs::kCachePrefPath);

  auto* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile_);
  if (!template_url_service ||
      !template_url_service->GetDefaultSearchProvider()) {
    return dictionary.size() > 0;
  }

  for (auto element : dictionary) {
    GURL navigation_url(net::SimplifyUrlForRequest(GURL(element.first)));
    if (!navigation_url.is_valid())
      continue;

    base::Value::ConstListView const prefetch_url_and_time =
        base::Value::AsListValue(element.second).GetListDeprecated();

    if (prefetch_url_and_time.size() != 2 ||
        !prefetch_url_and_time[0].is_string() ||
        !prefetch_url_and_time[1].is_string()) {
      continue;
    }

    const std::string* prefetch_url_string =
        prefetch_url_and_time[0].GetIfString();
    if (!prefetch_url_string)
      continue;

    GURL prefetch_url(net::SimplifyUrlForRequest(GURL(*prefetch_url_string)));
    // Make sure we are only mapping same origin in case of corrupted prefs.
    if (url::Origin::Create(navigation_url) !=
        url::Origin::Create(prefetch_url)) {
      continue;
    }

    // Don't redirect same URL.
    if (navigation_url == prefetch_url)
      continue;

    // Make sure the navigation URL is still a search URL.
    std::u16string search_terms;
    template_url_service->GetDefaultSearchProvider()->ExtractSearchTermsFromURL(
        navigation_url, template_url_service->search_terms_data(),
        &search_terms);

    if (search_terms.size() == 0) {
      continue;
    }

    absl::optional<base::Time> last_update =
        base::ValueToTime(prefetch_url_and_time[1]);
    if (!last_update) {
      continue;
    }

    // This time isn't valid.
    if (last_update.value() > base::Time::Now()) {
      continue;
    }

    prefetch_cache_.emplace(navigation_url,
                            std::make_pair(prefetch_url, last_update.value()));
  }
  return dictionary.size() > prefetch_cache_.size();
}

void SearchPrefetchService::SaveToPrefs() const {
  base::Value::Dict dictionary;
  for (const auto& element : prefetch_cache_) {
    std::string navigation_url = element.first.spec();
    std::string prefetch_url = element.second.first.spec();
    base::Value::List value;
    value.Append(prefetch_url);
    value.Append(base::TimeToValue(element.second.second));
    dictionary.Set(std::move(navigation_url), std::move(value));
  }
  profile_->GetPrefs()->Set(prefetch::prefs::kCachePrefPath,
                            base::Value(std::move(dictionary)));
}

bool SearchPrefetchService::LoadFromPrefsForTesting() {
  return LoadFromPrefs();
}

void SearchPrefetchService::ObserveTemplateURLService(
    TemplateURLService* template_url_service) {
  if (!observer_.IsObserving()) {
    observer_.Observe(template_url_service);

    template_url_service_data_ =
        template_url_service->GetDefaultSearchProvider()->data();

    omnibox_subscription_ =
        OmniboxEventGlobalTracker::GetInstance()->RegisterCallback(
            base::BindRepeating(&SearchPrefetchService::OnURLOpenedFromOmnibox,
                                base::Unretained(this)));
  }
}

void SearchPrefetchService::CoordinatePrefetchWithPrerender(
    const AutocompleteMatch& match,
    content::WebContents* web_contents,
    TemplateURLService* template_url_service) {
  DCHECK(web_contents);
  GURL prefetch_url = GetPreloadURLFromMatch(
      match, template_url_service, /*attach_prefetch_information=*/true);
  MaybePrefetchURL(prefetch_url);
  if (!BaseSearchProvider::ShouldPrerender(match))
    return;

  if (auto prefetch_request_iter =
          prefetches_.find(match.search_terms_args->search_terms);
      prefetch_request_iter != prefetches_.end()) {
    PrerenderManager::CreateForWebContents(web_contents);
    auto* prerender_manager = PrerenderManager::FromWebContents(web_contents);
    DCHECK(prerender_manager);

    // Prerender URL needs not to contain the prefetch information to help
    // servers to recognize prefetch traffic, because it should not send network
    // requests.
    GURL prerender_url = GetPreloadURLFromMatch(
        match, template_url_service, /*attach_prefetch_information=*/false);
    prefetch_request_iter->second->MaybeStartPrerenderSearchResult(
        *prerender_manager, prerender_url);
  }
}

std::map<std::u16string, std::unique_ptr<SearchPrefetchRequest>>::iterator
SearchPrefetchService::RetrieveSearchTermsInMemoryCache(
    const network::ResourceRequest& tentative_resource_request,
    SearchPrefetchServingReasonRecorder& recorder) {
  const GURL& navigation_url = tentative_resource_request.url;

  auto* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile_);
  if (!template_url_service ||
      !template_url_service->GetDefaultSearchProvider()) {
    recorder.reason_ = SearchPrefetchServingReason::kSearchEngineNotValid;
    return prefetches_.end();
  }

  // The user may have disabled JS since the prefetch occurred.
  if (!profile_->GetPrefs() ||
      !profile_->GetPrefs()->GetBoolean(prefs::kWebKitJavascriptEnabled)) {
    recorder.reason_ = SearchPrefetchServingReason::kJavascriptDisabled;
    return prefetches_.end();
  }

  auto* content_settings =
      HostContentSettingsMapFactory::GetForProfile(profile_);
  if (!content_settings ||
      content_settings->GetContentSetting(navigation_url, navigation_url,
                                          ContentSettingsType::JAVASCRIPT) ==
          CONTENT_SETTING_BLOCK) {
    recorder.reason_ = SearchPrefetchServingReason::kJavascriptDisabled;
    return prefetches_.end();
  }

  std::u16string search_terms;
  template_url_service->GetDefaultSearchProvider()->ExtractSearchTermsFromURL(
      navigation_url, template_url_service->search_terms_data(), &search_terms);

  if (search_terms.length() == 0) {
    recorder.reason_ = SearchPrefetchServingReason::kNotDefaultSearchWithTerms;
    return prefetches_.end();
  }

  const auto& iter = prefetches_.find(search_terms);

  if (iter == prefetches_.end()) {
    recorder.reason_ = SearchPrefetchServingReason::kNoPrefetch;
    return prefetches_.end();
  }

  // Verify that the URL is the same origin as the prefetch URL. While other
  // checks should address this by clearing prefetches on user changes to
  // default search, it is paramount to never serve content from one origin to
  // another.
  if (url::Origin::Create(navigation_url) !=
      url::Origin::Create(iter->second->prefetch_url())) {
    recorder.reason_ =
        SearchPrefetchServingReason::kPrefetchWasForDifferentOrigin;
    return prefetches_.end();
  }

  switch (iter->second->current_status()) {
    case SearchPrefetchStatus::kRequestCancelled:
      recorder.reason_ = SearchPrefetchServingReason::kRequestWasCancelled;
      break;
    case SearchPrefetchStatus::kRequestFailed:
      recorder.reason_ = SearchPrefetchServingReason::kRequestFailed;
      break;
    case SearchPrefetchStatus::kPrerendered:
      recorder.reason_ = SearchPrefetchServingReason::kPrerendered;
      break;
    default:
      break;
  }
  if (recorder.reason_ != SearchPrefetchServingReason::kServed)
    return prefetches_.end();

  // POST requests are not supported since they are non-idempotent. Only support
  // GET.
  if (tentative_resource_request.method !=
      net::HttpRequestHeaders::kGetMethod) {
    recorder.reason_ = SearchPrefetchServingReason::kPostReloadFormOrLink;
    return prefetches_.end();
  }

  // If the client requests disabling, bypassing, or validating cache, don't
  // return a prefetch.
  // These are used mostly for reloads and dev tools.
  if (tentative_resource_request.load_flags & net::LOAD_BYPASS_CACHE ||
      tentative_resource_request.load_flags & net::LOAD_DISABLE_CACHE ||
      tentative_resource_request.load_flags & net::LOAD_VALIDATE_CACHE) {
    recorder.reason_ = SearchPrefetchServingReason::kPostReloadFormOrLink;
    return prefetches_.end();
  }

  // Link clicks and form subbmit should not be served with a prefetch due to
  // results page nth page matching the URL pattern of the DSE.
  if (ui::PageTransitionCoreTypeIs(
          static_cast<ui::PageTransition>(
              tentative_resource_request.transition_type),
          ui::PAGE_TRANSITION_LINK) ||
      ui::PageTransitionCoreTypeIs(
          static_cast<ui::PageTransition>(
              tentative_resource_request.transition_type),
          ui::PAGE_TRANSITION_FORM_SUBMIT)) {
    recorder.reason_ = SearchPrefetchServingReason::kPostReloadFormOrLink;
    return prefetches_.end();
  }

  return iter;
}

void SearchPrefetchService::FireAllExpiryTimerForTesting() {
  while (!prefetch_expiry_timers_.empty()) {
    auto prefetch_expiry_timer_it = prefetch_expiry_timers_.begin();
    prefetch_expiry_timer_it->second->FireNow();
  }
}
