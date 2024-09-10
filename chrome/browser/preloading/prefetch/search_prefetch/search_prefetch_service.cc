// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/prefetch/search_prefetch/search_prefetch_service.h"

#include <iterator>
#include <memory>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/values_util.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/prefetch/pref_names.h"
#include "chrome/browser/preloading/chrome_preloading.h"
#include "chrome/browser/preloading/prefetch/search_prefetch/cache_alias_search_prefetch_url_loader.h"
#include "chrome/browser/preloading/prefetch/search_prefetch/field_trial_settings.h"
#include "chrome/browser/preloading/prefetch/search_prefetch/search_prefetch_request.h"
#include "chrome/browser/preloading/prefetch/search_prefetch/search_prefetch_url_loader.h"
#include "chrome/browser/preloading/prefetch/search_prefetch/streaming_search_prefetch_url_loader.h"
#include "chrome/browser/preloading/preloading_prefs.h"
#include "chrome/browser/preloading/prerender/prerender_manager.h"
#include "chrome/browser/preloading/prerender/prerender_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/search_engines/ui_thread_search_terms_data.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/omnibox/browser/base_search_provider.h"
#include "components/omnibox/browser/omnibox_log.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/preloading_data.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "net/base/load_flags.h"
#include "net/base/url_util.h"
#include "services/network/public/cpp/network_quality_tracker.h"
#include "services/network/public/cpp/resource_request.h"
#include "ui/base/page_transition_types.h"
#include "url/origin.h"

using omnibox::mojom::NavigationPredictor;

namespace {
void SetIsNavigationInDomainCallback(content::PreloadingData* preloading_data) {
  constexpr content::PreloadingPredictor kPredictors[] = {
      chrome_preloading_predictor::kDefaultSearchEngine,
      chrome_preloading_predictor::kOmniboxSearchSuggestDefaultMatch,
      chrome_preloading_predictor::kOmniboxMousePredictor,
      chrome_preloading_predictor::kOmniboxSearchPredictor,
      chrome_preloading_predictor::kOmniboxTouchDownPredictor};
  for (const auto& predictor : kPredictors) {
    preloading_data->SetIsNavigationInDomainCallback(
        predictor,
        base::BindRepeating(
            [](content::NavigationHandle* navigation_handle) -> bool {
              auto transition_type = navigation_handle->GetPageTransition();
              return (transition_type & ui::PAGE_TRANSITION_FROM_ADDRESS_BAR) &&
                     ui::PageTransitionCoreTypeIs(
                         transition_type,
                         ui::PageTransition::PAGE_TRANSITION_GENERATED) &&
                     ui::PageTransitionIsNewNavigation(transition_type);
            }));
  }
}

// Recomputes the destination URL for |match| with the updated prefetch
// information (does not modify |destination_url|). Passing true to
// |attach_prefetch_information| if the URL request will be sent to network,
// otherwise set to false if it is for client-internal use only.
GURL GetPreloadURLFromMatch(
    const TemplateURLRef::SearchTermsArgs& search_terms_args_from_match,
    TemplateURLService* template_url_service,
    std::string prefetch_param) {
  // Copy the search term args, so we can modify them for just the prefetch.
  auto search_terms_args = search_terms_args_from_match;
  search_terms_args.prefetch_param = prefetch_param;
  const TemplateURL* default_provider =
      template_url_service->GetDefaultSearchProvider();
  DCHECK(default_provider);
  GURL prefetch_url = GURL(default_provider->url_ref().ReplaceSearchTerms(
      search_terms_args, template_url_service->search_terms_data(), nullptr));
  return prefetch_url;
}

struct SearchPrefetchEligibilityReasonRecorder {
 public:
  explicit SearchPrefetchEligibilityReasonRecorder(bool navigation_prefetch)
      : navigation_prefetch_(navigation_prefetch) {}
  ~SearchPrefetchEligibilityReasonRecorder() {
    if (navigation_prefetch_) {
      UMA_HISTOGRAM_ENUMERATION(
          "Omnibox.SearchPrefetch.PrefetchEligibilityReason2."
          "NavigationPrefetch",
          reason_);
    } else {
      UMA_HISTOGRAM_ENUMERATION(
          "Omnibox.SearchPrefetch.PrefetchEligibilityReason2."
          "SuggestionPrefetch",
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

void SetEligibility(content::PreloadingAttempt* preloading_attempt,
                    content::PreloadingEligibility eligibility) {
  if (!preloading_attempt)
    return;

  preloading_attempt->SetEligibility(eligibility);
}

// Returns true when Prefetch is not in the holdback group.
bool CheckAndSetPrefetchHoldbackStatus(
    content::PreloadingAttempt* preloading_attempt) {
  // Return true as we only set and check for holdback when PreloadingAttempt is
  // created.
  if (!preloading_attempt)
    return true;

  // In addition to the globally-controlled preloading config, check for the
  // feature-specific holdback. We disable the feature if the user is in either
  // of those holdbacks.
  if (base::GetFieldTrialParamByFeatureAsBool(kSearchPrefetchServicePrefetching,
                                              "prefetch_holdback", false)) {
    preloading_attempt->SetHoldbackStatus(
        content::PreloadingHoldbackStatus::kHoldback);
  }
  if (preloading_attempt->ShouldHoldback()) {
    return false;
  }
  return true;
}

void SetTriggeringOutcome(content::PreloadingAttempt* preloading_attempt,
                          content::PreloadingTriggeringOutcome outcome) {
  if (!preloading_attempt)
    return;

  preloading_attempt->SetTriggeringOutcome(outcome);
}

content::PreloadingFailureReason ToPreloadingFailureReason(
    SearchPrefetchServingReason reason) {
  // If you are copying this pattern for another prefetch use case beyond
  // SearchPrefetchServingReason, please take care to ensure that you use a
  // non-overlapping range after kPreloadingFailureReasonContentEnd. It is
  // probably a good idea to centralize the allocation of enum ranges whenever a
  // second case emerges.
  // Ensure that the enums do not overlap.
  static_assert(static_cast<int>(SearchPrefetchServingReason::kServed) !=
                    static_cast<int>(content::PreloadingFailureReason::
                                         kPreloadingFailureReasonContentEnd),
                "Enum values overlap! Update enum values.");

  // Calculate and return the result.
  return static_cast<content::PreloadingFailureReason>(
      static_cast<int>(reason) +
      static_cast<int>(content::PreloadingFailureReason::
                           kPreloadingFailureReasonContentEnd));
}

bool IsSlowNetwork() {
  static const base::TimeDelta kSlowNetworkThreshold =
      kSuppressesSearchPrefetchOnSlowNetworkThreshold.Get();
  if (g_browser_process->network_quality_tracker() &&
      g_browser_process->network_quality_tracker()->GetHttpRTT() >
          kSlowNetworkThreshold) {
    return true;
  }
  return false;
}

}  // namespace

struct SearchPrefetchService::SearchPrefetchServingReasonRecorder {
 public:
  explicit SearchPrefetchServingReasonRecorder(bool for_prerender)
      : for_prerender_(for_prerender) {}
  ~SearchPrefetchServingReasonRecorder() {
    base::UmaHistogramEnumeration(
        for_prerender_
            ? "Omnibox.SearchPrefetch.PrefetchServingReason2.Prerender"
            : "Omnibox.SearchPrefetch.PrefetchServingReason2",
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

bool SearchPrefetchService::MaybePrefetchURL(
    const GURL& url,
    content::WebContents* web_contents) {
  return MaybePrefetchURL(url, /*navigation_prefetch=*/false, web_contents,
                          chrome_preloading_predictor::kDefaultSearchEngine);
}

bool SearchPrefetchService::MaybePrefetchURL(
    const GURL& url,
    bool navigation_prefetch,
    content::WebContents* web_contents,
    content::PreloadingPredictor predictor) {
  if (!SearchPrefetchServicePrefetchingIsEnabled())
    return false;

  SearchPrefetchEligibilityReasonRecorder recorder(navigation_prefetch);

  // Check for search terms before checking for any other eligibility reasons
  // for Prefetch to exit early. And extract the canonical search URL.
  auto* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile_);
  if (!template_url_service ||
      !template_url_service->GetDefaultSearchProvider()) {
    recorder.reason_ = SearchPrefetchEligibilityReason::kSearchEngineNotValid;
    return false;
  }

  // Lazily observe Template URL Service.
  ObserveTemplateURLService(template_url_service);

  GURL canonical_search_url;
  bool search_with_terms = HasCanonicalPreloadingOmniboxSearchURL(
      url, profile_, &canonical_search_url);

  // It is possible that the current page doesn't exist. Don't create
  // PreloadingAttempt in that case.
  content::PreloadingAttempt* attempt = nullptr;
  DCHECK(web_contents);
  content::PreloadingURLMatchCallback same_url_matcher =
      base::BindRepeating(&IsSearchDestinationMatch, canonical_search_url,
                          web_contents->GetBrowserContext());

  auto* preloading_data =
      content::PreloadingData::GetOrCreateForWebContents(web_contents);
  SetIsNavigationInDomainCallback(preloading_data);
  // Create new PreloadingAttempt and pass all the values corresponding to
  // this DefaultSearchEngine or OmniboxSearchPredictor prefetch attempt when
  // |navigation_prefetch| is true.
  attempt = preloading_data->AddPreloadingAttempt(
      predictor, content::PreloadingType::kPrefetch, same_url_matcher,
      // Note that it'd be nice to use kPrerender if
      // `(!navigation_prefetch &&
      // prerender_utils::IsSearchSuggestionPrerenderEnabled())`. But currently
      // this attribute is not used for search preloads as expected behavior
      // varies depending on how this is triggered as follows:
      //
      // - If `navigation_prefetch` is true, we will not upgrade the attempt.
      // - If the default search engine prerender is not enabled, we will not
      // upgrade this attempt.
      // - If the server side does not ask to upgrade the request, we will not
      // upgrade it.
      /*planned_max_preloading_type=*/std::nullopt,
      web_contents->GetPrimaryMainFrame()->GetPageUkmSourceId());

  if (!search_with_terms) {
    recorder.reason_ =
        SearchPrefetchEligibilityReason::kNotDefaultSearchWithTerms;
    SetEligibility(attempt, ToPreloadingEligibility(
                                ChromePreloadingEligibility::kNoSearchTerms));
    return false;
  }

  auto eligibility = prefetch::IsSomePreloadingEnabled(*profile_->GetPrefs());
  if (eligibility != content::PreloadingEligibility::kEligible) {
    recorder.reason_ = SearchPrefetchEligibilityReason::kPrefetchDisabled;
    SetEligibility(attempt, eligibility);
    return false;
  }

  if (!profile_->GetPrefs() ||
      !profile_->GetPrefs()->GetBoolean(prefs::kWebKitJavascriptEnabled)) {
    recorder.reason_ = SearchPrefetchEligibilityReason::kJavascriptDisabled;
    SetEligibility(attempt,
                   content::PreloadingEligibility::kJavascriptDisabled);
    return false;
  }

  auto* content_settings =
      HostContentSettingsMapFactory::GetForProfile(profile_);
  if (!content_settings ||
      content_settings->GetContentSetting(
          url, url, ContentSettingsType::JAVASCRIPT) == CONTENT_SETTING_BLOCK) {
    recorder.reason_ = SearchPrefetchEligibilityReason::kJavascriptDisabled;
    SetEligibility(attempt,
                   content::PreloadingEligibility::kJavascriptDisabled);
    return false;
  }

  static const bool kSuppressesSearchPrefetchOnSlowNetworkIsEnabled =
      base::FeatureList::IsEnabled(kSuppressesSearchPrefetchOnSlowNetwork);
  if (kSuppressesSearchPrefetchOnSlowNetworkIsEnabled && IsSlowNetwork()) {
    recorder.reason_ = SearchPrefetchEligibilityReason::kSlowNetwork;
    SetEligibility(attempt, content::PreloadingEligibility::kSlowNetwork);
    return false;
  }

  // Prefetch has completed all the eligibility checks. Set the
  // PreloadingEligibility to kEligible.
  SetEligibility(attempt, content::PreloadingEligibility::kEligible);

  // Don't trigger prefetch if it is in holdback group. We do this after all the
  // eligibility checks to ensure we replicate the behaviour between our
  // experiment groups.
  if (!CheckAndSetPrefetchHoldbackStatus(attempt)) {
    return false;
  }

  if (last_error_time_ticks_ + SearchPrefetchErrorBackoffDuration() >
      base::TimeTicks::Now()) {
    recorder.reason_ = SearchPrefetchEligibilityReason::kErrorBackoff;
    // Recorded as a triggering outcome as it is based on a previous failures,
    // which cannot happen in a holdback arm.
    SetTriggeringOutcome(attempt,
                         content::PreloadingTriggeringOutcome::kFailure);
    return false;
  }

  // Don't prefetch the same search terms twice within the expiry duration.
  if (prefetches_.find(canonical_search_url) != prefetches_.end()) {
    recorder.reason_ = SearchPrefetchEligibilityReason::kAttemptedQueryRecently;
    // Prefetch was eligible as it was attempted recently but mark it as a
    // duplicate attempt.
    SetTriggeringOutcome(attempt,
                         content::PreloadingTriggeringOutcome::kDuplicate);
    return false;
  }

  if (prefetches_.size() >= SearchPrefetchMaxAttemptsPerCachingDuration()) {
    recorder.reason_ = SearchPrefetchEligibilityReason::kMaxAttemptsReached;
    // The reason we don't consider limit exceeded as an ineligibility
    // reason is because we can't replicate the behavior in our other
    // experiment groups for analysis. To prevent this we set
    // TriggeringOutcome to kFailure and look into the failure reason to
    // learn more.
    SetTriggeringOutcome(attempt,
                         content::PreloadingTriggeringOutcome::kFailure);
    return false;
  }

  std::unique_ptr<SearchPrefetchRequest> prefetch_request =
      std::make_unique<SearchPrefetchRequest>(
          canonical_search_url, url, navigation_prefetch, attempt,
          base::BindOnce(&SearchPrefetchService::ReportFetchResult,
                         base::Unretained(this)));

  DCHECK(prefetch_request);
  if (!prefetch_request->StartPrefetchRequest(profile_)) {
    recorder.reason_ = SearchPrefetchEligibilityReason::kThrottled;
    // We don't consider Throttled as an ineligibility reason is because we
    // can't replicate this behaviour in our other experiment group. To prevent
    // this we set TriggeringOutcome to kFailure and look into the failure
    // reason to learn more.
    SetTriggeringOutcome(attempt,
                         content::PreloadingTriggeringOutcome::kFailure);
    return false;
  }

  prefetches_.emplace(canonical_search_url, std::move(prefetch_request));
  prefetch_expiry_timers_.emplace(canonical_search_url,
                                  std::make_unique<base::OneShotTimer>());
  prefetch_expiry_timers_[canonical_search_url]->Start(
      FROM_HERE, SearchPrefetchCachingLimit(),
      base::BindOnce(&SearchPrefetchService::DeletePrefetch,
                     base::Unretained(this), canonical_search_url));
  return true;
}

void SearchPrefetchService::OnURLOpenedFromOmnibox(OmniboxLog* log) {
  if (!log) {
    return;
  }
  const GURL& opened_url = log->final_destination_url;

  auto& match = log->result->match_at(log->selection.line);
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
  if (!default_search) {
    return;
  }

  GURL canonical_search_url;

  HasCanonicalPreloadingOmniboxSearchURL(opened_url, profile_,
                                         &canonical_search_url);

  if (prefetches_.find(canonical_search_url) == prefetches_.end()) {
    return;
  }
  SearchPrefetchRequest& prefetch = *prefetches_[canonical_search_url];
  prefetch.RecordClickTime();
}

void SearchPrefetchService::OnPrerenderedRequestUsed(
    const GURL& canonical_search_url,
    const GURL& navigation_url) {
  auto request_it = prefetches_.find(canonical_search_url);
  DCHECK(request_it != prefetches_.end());
  if (request_it == prefetches_.end()) {
    // TODO(crbug.com/40214220): It should be rare but the request can be
    // deleted by timer before chrome activates the page. Add some metrics to
    // understand the possibility.
    return;
  }
  AddCacheEntry(navigation_url, request_it->second->prefetch_url());
  DeletePrefetch(canonical_search_url);
}

SearchPrefetchURLLoader::RequestHandler
SearchPrefetchService::MaybeCreateResponseReader(
    const network::ResourceRequest& tentative_resource_request) {
  SearchPrefetchServingReasonRecorder recorder{/*for_prerender=*/true};
  auto iter =
      RetrieveSearchTermsInMemoryCache(tentative_resource_request, recorder);
  if (iter == prefetches_.end()) {
    return {};
  }
  DCHECK_NE(iter->second->current_status(),
            SearchPrefetchStatus::kRequestFailed);
  return iter->second->CreateResponseReader();
}

std::optional<SearchPrefetchStatus>
SearchPrefetchService::GetSearchPrefetchStatusForTesting(
    const GURL& canonical_search_url) {
  if (prefetches_.find(canonical_search_url) == prefetches_.end()) {
    return std::nullopt;
  }
  return prefetches_[canonical_search_url]->current_status();
}

GURL SearchPrefetchService::GetRealPrefetchUrlForTesting(
    const GURL& canonical_search_url) {
  if (prefetches_.find(canonical_search_url) == prefetches_.end()) {
    return GURL();
  }
  return prefetches_[canonical_search_url]->prefetch_url();
}

SearchPrefetchURLLoader::RequestHandler
SearchPrefetchService::TakePrefetchResponseFromMemoryCache(
    const network::ResourceRequest& tentative_resource_request) {
  const GURL& navigation_url = tentative_resource_request.url;
  SearchPrefetchServingReasonRecorder recorder(/*for_prerender=*/false);

  auto iter =
      RetrieveSearchTermsInMemoryCache(tentative_resource_request, recorder);
  if (iter == prefetches_.end()) {
    DCHECK_NE(recorder.reason_, SearchPrefetchServingReason::kServed);
    return {};
  }

  auto status = iter->second->current_status();

  if (status == SearchPrefetchStatus::kInFlight) {
    recorder.reason_ = SearchPrefetchServingReason::kRequestInFlightNotReady;
    // Set the failure reason when prefetch is not served.
    iter->second->SetPrefetchAttemptFailureReason(ToPreloadingFailureReason(
        SearchPrefetchServingReason::kRequestInFlightNotReady));
    return {};
  }

  bool is_servable =
      status == SearchPrefetchStatus::kComplete ||
      status == SearchPrefetchStatus::kCanBeServed;

  if (!is_servable) {
    recorder.reason_ = SearchPrefetchServingReason::kNotServedOtherReason;
    // Set the failure reason when prefetch is not served.
    iter->second->SetPrefetchAttemptFailureReason(ToPreloadingFailureReason(
        SearchPrefetchServingReason::kNotServedOtherReason));
    return {};
  }

  scoped_refptr<StreamingSearchPrefetchURLLoader> loader =
      iter->second->TakeSearchPrefetchURLLoader();

  iter->second->MarkPrefetchAsServed();

  if (navigation_url != iter->second->prefetch_url()) {
    AddCacheEntry(navigation_url, iter->second->prefetch_url());
  }
  DeletePrefetch(iter->first);
  return StreamingSearchPrefetchURLLoader::GetServingResponseHandler(
      std::move(loader));
}

SearchPrefetchURLLoader::RequestHandler
SearchPrefetchService::TakePrefetchResponseFromDiskCache(
    const GURL& navigation_url) {
  GURL navigation_url_without_ref(net::SimplifyUrlForRequest(navigation_url));
  if (prefetch_cache_.find(navigation_url_without_ref) ==
      prefetch_cache_.end()) {
    return {};
  }

  auto loader = std::make_unique<CacheAliasSearchPrefetchURLLoader>(
      profile_, SearchPrefetchRequest::NetworkAnnotationForPrefetch(),
      prefetch_cache_[navigation_url_without_ref].first);
  return CacheAliasSearchPrefetchURLLoader::GetServingResponseHandlerFromLoader(
      std::move(loader));
}

void SearchPrefetchService::ClearPrefetches() {
  prefetches_.clear();
  prefetch_expiry_timers_.clear();
  prefetch_cache_.clear();
  SaveToPrefs();
}

void SearchPrefetchService::DeletePrefetch(GURL canonical_search_url) {
  DCHECK(prefetches_.find(canonical_search_url) != prefetches_.end());
  DCHECK(prefetch_expiry_timers_.find(canonical_search_url) !=
         prefetch_expiry_timers_.end());

  std::unique_ptr<SearchPrefetchRequest> request =
      std::move(prefetches_[canonical_search_url]);

  RecordFinalStatus(request->current_status(), request->navigation_prefetch());

  prefetches_.erase(canonical_search_url);
  prefetch_expiry_timers_.erase(canonical_search_url);
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

  // Don't cancel unneeded prefetch requests, but reset all pending prerenders.
  // It will be set soon if service still wants clients to prerender a
  // SearchTerms.
  // TODO(crbug.com/40214220): Unlike prefetch, which does not discard completed
  // response to avoid wasting, prerender would like to cancel itself given the
  // cost of a prerender. For now prenderer is canceled when the prerender hints
  // changed, we need to revisit this decision.
  for (const auto& kv_pair : prefetches_) {
    auto& prefetch_request = kv_pair.second;
    prefetch_request->ResetPrerenderUpgrader();
  }

  // Do not perform preloading if there is no active tab.
  if (!web_contents)
    return;

  // This preloads dictionaries for AutocompleteResult's `destination_url` which
  // are not specific to search prefetch.
  // TODO(crbug.com/349030549): Consider moving somewhere more suitable.
  MaybePreloadDictionary(result);

  for (const auto& match : result) {
    // Return early if neither prefetch nor prerender are enabled for the match.
    if (!ShouldPrefetch(match)) {
      continue;
    }

    // In the case of Default Search Engine Prediction, the confidence depends
    // on the type of preloading. For prerender requests, the confidence is
    // comparatively higher than the prefetch to avoid the impact of wrong
    // predictions. We set confidence as 80 for prerender matches and 60 for
    // prefetch as an approximate number to differentiate both these cases.
    int confidence = BaseSearchProvider::ShouldPrerender(match) ? 80 : 60;
    auto* preloading_data =
        content::PreloadingData::GetOrCreateForWebContents(web_contents);
    SetIsNavigationInDomainCallback(preloading_data);
    GURL canonical_search_url;
    HasCanonicalPreloadingOmniboxSearchURL(match.destination_url, profile_,
                                           &canonical_search_url);

    content::PreloadingURLMatchCallback same_url_matcher =
        base::BindRepeating(&IsSearchDestinationMatch, canonical_search_url,
                            web_contents->GetBrowserContext());

    ukm::SourceId triggered_primary_page_source_id =
        web_contents->GetPrimaryMainFrame()->GetPageUkmSourceId();
    // Create PreloadingPrediction for this match.
    preloading_data->AddPreloadingPrediction(
        chrome_preloading_predictor::kDefaultSearchEngine, confidence,
        std::move(same_url_matcher), triggered_primary_page_source_id);

    // Record a prediction for default match prefetch suggest predictions.
    if (result.default_match() == &match) {
      preloading_data =
          content::PreloadingData::GetOrCreateForWebContents(web_contents);

      same_url_matcher =
          base::BindRepeating(&IsSearchDestinationMatch, canonical_search_url,
                              web_contents->GetBrowserContext());

      // Create PreloadingPrediction for this match.
      preloading_data->AddPreloadingPrediction(
          chrome_preloading_predictor::kOmniboxSearchSuggestDefaultMatch,
          confidence, std::move(same_url_matcher),
          triggered_primary_page_source_id);
    } else if (OnlyAllowDefaultMatchPreloading()) {
      // Only prefetch default match when in the experiment.
      continue;
    }

    if (prerender_utils::IsSearchSuggestionPrerenderEnabled()) {
      CoordinatePrefetchWithPrerender(match, web_contents, template_url_service,
                                      canonical_search_url);
      continue;
    }

    if (BaseSearchProvider::ShouldPrefetch(match)) {
      MaybePrefetchURL(
          GetPreloadURLFromMatch(*match.search_terms_args, template_url_service,
                                 kSuggestPrefetchParam.Get()),
          web_contents);
    }
  }
}

bool SearchPrefetchService::OnNavigationLikely(
    size_t index,
    const AutocompleteMatch& match,
    NavigationPredictor navigation_predictor,
    content::WebContents* web_contents) {
  if (!IsSearchNavigationPrefetchEnabled())
    return false;

  auto is_type_allowed = [](NavigationPredictor navigation_predictor) {
    switch (navigation_predictor) {
      case NavigationPredictor::kMouseDown:
        return IsSearchMouseDownPrefetchEnabled();
      case NavigationPredictor::kUpOrDownArrowButton:
        return IsUpOrDownArrowPrefetchEnabled();
      case NavigationPredictor::kTouchDown:
        return IsTouchDownPrefetchEnabled();
    }
  };

  if (!is_type_allowed(navigation_predictor)) {
    return false;
  }

  if (!web_contents)
    return false;
  if (!AllowTopNavigationPrefetch() && index == 0)
    return false;
  // Only prefetch search types.
  if (!AutocompleteMatch::IsSearchType(match.type))
    return false;
  // Check to make sure this is search related and that we can read the search
  // arguments. For Search history this may be null.

  auto* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile_);
  // The default search provider needs to opt into prefetching behavior.
  if (!template_url_service ||
      !template_url_service->GetDefaultSearchProvider() ||
      !template_url_service->GetDefaultSearchProvider()
           ->data()
           .prefetch_likely_navigations) {
    return false;
  }

  GURL canonical_search_url;
  if (!HasCanonicalPreloadingOmniboxSearchURL(match.destination_url, profile_,
                                              &canonical_search_url)) {
    return false;
  }

  // Parse the search terms from the match URL to verify this is a valid search
  // query.
  std::u16string search_terms;
  template_url_service->GetDefaultSearchProvider()->ExtractSearchTermsFromURL(
      match.destination_url, template_url_service->search_terms_data(),
      &search_terms);

  if (search_terms.size() == 0)
    return false;

  // Search history suggestions (those that are not also server suggestions)
  // don't have search term args. If search history suggestions are enabled,
  // generate search term args to get a prefetch URL.
  TemplateURLRef::SearchTermsArgs* search_terms_args_for_prefetch;
  std::unique_ptr<TemplateURLRef::SearchTermsArgs> search_terms_args;
  if (!match.search_terms_args) {
    if (!PrefetchSearchHistorySuggestions())
      return false;
    search_terms_args =
        std::make_unique<TemplateURLRef::SearchTermsArgs>(search_terms);
    search_terms_args_for_prefetch = search_terms_args.get();
  } else {
    search_terms_args_for_prefetch = match.search_terms_args.get();
  }

  GURL preload_url = GetPreloadURLFromMatch(*search_terms_args_for_prefetch,
                                            template_url_service,
                                            kNavigationPrefetchParam.Get());

  content::PreloadingURLMatchCallback same_url_matcher =
      base::BindRepeating(&IsSearchDestinationMatch, canonical_search_url,
                          web_contents->GetBrowserContext());
  auto* preloading_data =
      content::PreloadingData::GetOrCreateForWebContents(web_contents);

  auto navigation_likely_event_to_predictor =
      [](NavigationPredictor navigation_predictor) {
        switch (navigation_predictor) {
          case NavigationPredictor::kMouseDown:
            return chrome_preloading_predictor::kOmniboxMousePredictor;
          case NavigationPredictor::kUpOrDownArrowButton:
            return chrome_preloading_predictor::kOmniboxSearchPredictor;
          case NavigationPredictor::kTouchDown:
            return chrome_preloading_predictor::kOmniboxTouchDownPredictor;
        }
      };
  auto predictor = navigation_likely_event_to_predictor(navigation_predictor);
  SetIsNavigationInDomainCallback(preloading_data);
  // Create PreloadingPrediction for this match. We set the confidence to 100 as
  // when the user changed the selected match, we always trigger prefetch.
  preloading_data->AddPreloadingPrediction(
      predictor, 100, std::move(same_url_matcher),
      web_contents->GetPrimaryMainFrame()->GetPageUkmSourceId());

  base::TimeTicks prefetch_started_time_stamp = base::TimeTicks::Now();
  bool was_prefetch_started =
      MaybePrefetchURL(preload_url,
                       /*navigation_prefetch=*/true, web_contents, predictor);
  if (was_prefetch_started) {
    UMA_HISTOGRAM_TIMES("Omnibox.SearchPrefetch.StartTimeV2.NavigationPrefetch",
                        (base::TimeTicks::Now() - prefetch_started_time_stamp));
  }
  return was_prefetch_started;
}

void SearchPrefetchService::OnTemplateURLServiceChanged() {
  auto* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile_);
  DCHECK(template_url_service);

  std::optional<TemplateURLData> template_url_service_data;

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
      profile_->GetPrefs()->GetDict(prefetch::prefs::kCachePrefPath);

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

    const base::Value::List& prefetch_url_and_time = element.second.GetList();

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

    std::optional<base::Time> last_update =
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

    // The max size of the cache entries can be changed from the previous
    // session. Stop loading the entries if the limit is reached.
    // TODO(crbug.com/353628436): We may want to prioritize newer entries.
    if (prefetch_cache_.size() == SearchPrefetchMaxCacheEntries()) {
      break;
    }
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

    const TemplateURL* default_provider =
        template_url_service->GetDefaultSearchProvider();
    DCHECK(default_provider);
    template_url_service_data_ = default_provider->data();
  }
}

void SearchPrefetchService::CoordinatePrefetchWithPrerender(
    const AutocompleteMatch& match,
    content::WebContents* web_contents,
    TemplateURLService* template_url_service,
    const GURL& canonical_search_url) {
  DCHECK(web_contents);
  GURL prefetch_url =
      GetPreloadURLFromMatch(*match.search_terms_args, template_url_service,
                             kSuggestPrefetchParam.Get());
  MaybePrefetchURL(prefetch_url, web_contents);
  if (!BaseSearchProvider::ShouldPrerender(match))
    return;

  content::PreloadingURLMatchCallback same_url_matcher =
      base::BindRepeating(&IsSearchDestinationMatch, canonical_search_url,
                          web_contents->GetBrowserContext());

  // Create new PreloadingAttempt and pass all the values corresponding to
  // this prerendering attempt.
  auto* preloading_data =
      content::PreloadingData::GetOrCreateForWebContents(web_contents);
  SetIsNavigationInDomainCallback(preloading_data);
  content::PreloadingAttempt* preloading_attempt =
      preloading_data->AddPreloadingAttempt(
          chrome_preloading_predictor::kDefaultSearchEngine,
          content::PreloadingType::kPrerender, same_url_matcher,
          /*planned_max_preloading_type=*/std::nullopt,
          web_contents->GetPrimaryMainFrame()->GetPageUkmSourceId());

  auto prefetch_request_iter = prefetches_.find(canonical_search_url);
  if (prefetch_request_iter == prefetches_.end()) {
    preloading_attempt->SetEligibility(ToPreloadingEligibility(
        ChromePreloadingEligibility::kPrefetchNotStarted));
    return;
  }

  PrerenderManager::CreateForWebContents(web_contents);
  auto* prerender_manager = PrerenderManager::FromWebContents(web_contents);
  DCHECK(prerender_manager);

  // Prerender URL need not contain the prefetch information to help servers to
  // recognize prefetch traffic, because it should not send network requests.
  GURL prerender_url =
      GetPreloadURLFromMatch(*match.search_terms_args, template_url_service,
                             /*prefetch_param=*/"");
  prefetch_request_iter->second->MaybeStartPrerenderSearchResult(
      *prerender_manager, prerender_url, *preloading_attempt);
}

std::map<GURL, std::unique_ptr<SearchPrefetchRequest>>::iterator
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

  GURL canonical_search_url;
  if (!HasCanonicalPreloadingOmniboxSearchURL(navigation_url, profile_,
                                              &canonical_search_url) ||
      !IsSearchDestinationMatch(canonical_search_url, profile_,
                                navigation_url)) {
    recorder.reason_ = SearchPrefetchServingReason::kNotDefaultSearchWithTerms;
    return prefetches_.end();
  }

  const auto& iter = prefetches_.find(canonical_search_url);

  // Return early if there is no prefetch found before checking for other
  // reasons.
  if (iter == prefetches_.end()) {
    recorder.reason_ = SearchPrefetchServingReason::kNoPrefetch;
    return prefetches_.end();
  }

  // The user may have disabled JS since the prefetch occurred.
  if (!profile_->GetPrefs() ||
      !profile_->GetPrefs()->GetBoolean(prefs::kWebKitJavascriptEnabled)) {
    recorder.reason_ = SearchPrefetchServingReason::kJavascriptDisabled;
    // Set the corresponding failure reason.
    iter->second->SetPrefetchAttemptFailureReason(ToPreloadingFailureReason(
        SearchPrefetchServingReason::kJavascriptDisabled));
    return prefetches_.end();
  }

  auto* content_settings =
      HostContentSettingsMapFactory::GetForProfile(profile_);
  if (!content_settings ||
      content_settings->GetContentSetting(navigation_url, navigation_url,
                                          ContentSettingsType::JAVASCRIPT) ==
          CONTENT_SETTING_BLOCK) {
    recorder.reason_ = SearchPrefetchServingReason::kJavascriptDisabled;
    // Set the corresponding failure reason.
    iter->second->SetPrefetchAttemptFailureReason(ToPreloadingFailureReason(
        SearchPrefetchServingReason::kJavascriptDisabled));
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
    // Set the corresponding failure reason.
    iter->second->SetPrefetchAttemptFailureReason(ToPreloadingFailureReason(
        SearchPrefetchServingReason::kPrefetchWasForDifferentOrigin));
    return prefetches_.end();
  }

  switch (iter->second->current_status()) {
    case SearchPrefetchStatus::kRequestFailed:
      recorder.reason_ = SearchPrefetchServingReason::kRequestFailed;
      // Set the corresponding failure reason.
      iter->second->SetPrefetchAttemptFailureReason(ToPreloadingFailureReason(
          SearchPrefetchServingReason::kRequestFailed));
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
    // Set the corresponding failure reason.
    iter->second->SetPrefetchAttemptFailureReason(ToPreloadingFailureReason(
        SearchPrefetchServingReason::kPostReloadFormOrLink));
    return prefetches_.end();
  }

  // If the client requests disabling, bypassing, or validating cache, don't
  // return a prefetch.
  // These are used mostly for reloads and dev tools.
  if (tentative_resource_request.load_flags & net::LOAD_BYPASS_CACHE ||
      tentative_resource_request.load_flags & net::LOAD_DISABLE_CACHE ||
      tentative_resource_request.load_flags & net::LOAD_VALIDATE_CACHE) {
    recorder.reason_ = SearchPrefetchServingReason::kPostReloadFormOrLink;
    // Set the corresponding failure reason.
    iter->second->SetPrefetchAttemptFailureReason(ToPreloadingFailureReason(
        SearchPrefetchServingReason::kPostReloadFormOrLink));

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
    // Set the corresponding failure reason.
    iter->second->SetPrefetchAttemptFailureReason(ToPreloadingFailureReason(
        SearchPrefetchServingReason::kPostReloadFormOrLink));
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

void SearchPrefetchService::SetLoaderDestructionCallbackForTesting(
    const GURL& canonical_search_url,
    base::OnceClosure streaming_url_loader_destruction_callback) {
  CHECK(base::Contains(prefetches_, canonical_search_url));
  return prefetches_[canonical_search_url]
      ->SetLoaderDestructionCallbackForTesting(  // IN-TEST
          std::move(streaming_url_loader_destruction_callback));
}

void SearchPrefetchService::MaybePreloadDictionary(
    const AutocompleteResult& result) {
  if (!base::FeatureList::IsEnabled(kAutocompleteDictionaryPreload)) {
    return;
  }
  std::vector<GURL> match_destination_urls;
  match_destination_urls.reserve(result.size());
  for (const AutocompleteMatch& match : result) {
    if (match.destination_url.SchemeIsHTTPOrHTTPS()) {
      match_destination_urls.emplace_back(match.destination_url);
    }
  }

  if (match_destination_urls.empty()) {
    return;
  }

  // Keep the old handle until `PreloadSharedDictionaryInfoForDocument()` call
  // to avoid reloading dictionaries in the network service.
  mojo::PendingRemote<network::mojom::PreloadedSharedDictionaryInfoHandle>
      old_handle = std::move(preloaded_shared_dictionaries_handle_);

  preloaded_shared_dictionaries_handle_.reset();
  profile_->GetDefaultStoragePartition()
      ->GetNetworkContext()
      ->PreloadSharedDictionaryInfoForDocument(
          match_destination_urls, preloaded_shared_dictionaries_handle_
                                      .InitWithNewPipeAndPassReceiver());
  preloaded_shared_dictionaries_expiry_timer_.Start(
      FROM_HERE, kAutocompletePreloadedDictionaryTimeout.Get(),
      base::BindOnce(&SearchPrefetchService::DeletePreloadedDictionaries,
                     base::Unretained(this)));
}

void SearchPrefetchService::DeletePreloadedDictionaries() {
  preloaded_shared_dictionaries_handle_.reset();
}
