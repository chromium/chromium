// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefetch/search_prefetch/search_prefetch_service.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/json/values_util.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/values.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/prefetch/pref_names.h"
#include "chrome/browser/prefetch/prefetch_prefs.h"
#include "chrome/browser/prefetch/search_prefetch/cache_alias_search_prefetch_url_loader.h"
#include "chrome/browser/prefetch/search_prefetch/field_trial_settings.h"
#include "chrome/browser/prefetch/search_prefetch/search_prefetch_url_loader.h"
#include "chrome/browser/prefetch/search_prefetch/streaming_search_prefetch_request.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/search_engines/ui_thread_search_terms_data.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
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
#include "url/origin.h"

namespace {

// Recomputes the destination URL with the added prefetch information for
// |match| (does not modify |destination_url|).
GURL GetPrefetchURLFromMatch(const AutocompleteMatch& match,
                             TemplateURLService* template_url_service) {
  // Copy the search term args, so we can modify them for just the prefetch.
  auto search_terms_args = *(match.search_terms_args);
  search_terms_args.is_prefetch = true;
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

struct SearchPrefetchServingReasonRecorder {
 public:
  SearchPrefetchServingReasonRecorder() = default;
  ~SearchPrefetchServingReasonRecorder() {
    UMA_HISTOGRAM_ENUMERATION("Omnibox.SearchPrefetch.PrefetchServingReason",
                              reason_);
  }

  SearchPrefetchServingReason reason_ = SearchPrefetchServingReason::kServed;
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

}  // namespace

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
        status == SearchPrefetchStatus::kComplete) {
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

  std::unique_ptr<BaseSearchPrefetchRequest> prefetch_request =
      std::make_unique<StreamingSearchPrefetchRequest>(
          url, navigation_prefetch,
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

  if (IsSearchNavigationPrefetchEnabled() &&
      default_search->data().prefetch_likely_navigations) {
    auto start = base::TimeTicks::Now();
    bool started_prefetch = MaybePrefetchURL(opened_url,
                                             /*navigation_prefetch=*/true);

    // Record the overhead of starting the prefetch earlier.
    if (started_prefetch) {
      UMA_HISTOGRAM_TIMES("Omnibox.SearchPrefetch.StartTime.NavigationPrefetch",
                          (base::TimeTicks::Now() - start));
    }
  }

  if (prefetches_.find(match_search_terms) == prefetches_.end()) {
    return;
  }
  BaseSearchPrefetchRequest& prefetch = *prefetches_[match_search_terms];
  prefetch.RecordClickTime();
  if (prefetch.current_status() != SearchPrefetchStatus::kCanBeServed) {
    return;
  }
  prefetch.MarkPrefetchAsClicked();
}

void SearchPrefetchService::AddCacheEntryForPrerender(
    const GURL& updated_prerendered_url,
    const GURL& prerendering_url) {
  DCHECK(prerender_utils::IsSearchSuggestionPrerenderEnabled());
  AddCacheEntry(updated_prerendered_url, prerendering_url);
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
  SearchPrefetchServingReasonRecorder recorder;

  auto* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile_);
  if (!template_url_service ||
      !template_url_service->GetDefaultSearchProvider()) {
    recorder.reason_ = SearchPrefetchServingReason::kSearchEngineNotValid;
    return nullptr;
  }

  // The user may have disabled JS since the prefetch occured.
  if (!profile_->GetPrefs() ||
      !profile_->GetPrefs()->GetBoolean(prefs::kWebKitJavascriptEnabled)) {
    recorder.reason_ = SearchPrefetchServingReason::kJavascriptDisabled;
    return nullptr;
  }

  auto* content_settings =
      HostContentSettingsMapFactory::GetForProfile(profile_);
  if (!content_settings ||
      content_settings->GetContentSetting(navigation_url, navigation_url,
                                          ContentSettingsType::JAVASCRIPT) ==
          CONTENT_SETTING_BLOCK) {
    recorder.reason_ = SearchPrefetchServingReason::kJavascriptDisabled;
    return nullptr;
  }

  std::u16string search_terms;
  template_url_service->GetDefaultSearchProvider()->ExtractSearchTermsFromURL(
      navigation_url, template_url_service->search_terms_data(), &search_terms);

  if (search_terms.length() == 0) {
    recorder.reason_ = SearchPrefetchServingReason::kNotDefaultSearchWithTerms;
    return nullptr;
  }

  const auto& iter = prefetches_.find(search_terms);

  if (iter == prefetches_.end()) {
    recorder.reason_ = SearchPrefetchServingReason::kNoPrefetch;
    return nullptr;
  }

  // Verify that the URL is the same origin as the prefetch URL. While other
  // checks should address this by clearing prefetches on user changes to
  // default search, it is paramount to never serve content from one origin to
  // another.
  if (url::Origin::Create(navigation_url) !=
      url::Origin::Create(iter->second->prefetch_url())) {
    recorder.reason_ =
        SearchPrefetchServingReason::kPrefetchWasForDifferentOrigin;
    return nullptr;
  }

  if (iter->second->current_status() ==
      SearchPrefetchStatus::kRequestCancelled) {
    recorder.reason_ = SearchPrefetchServingReason::kRequestWasCancelled;
    return nullptr;
  }

  if (iter->second->current_status() == SearchPrefetchStatus::kRequestFailed) {
    recorder.reason_ = SearchPrefetchServingReason::kRequestFailed;
    return nullptr;
  }

  // POST requests are not supported since they are non-idempotent. Only support
  // GET.
  if (tentative_resource_request.method !=
      net::HttpRequestHeaders::kGetMethod) {
    recorder.reason_ = SearchPrefetchServingReason::kPostReloadOrLink;
    return nullptr;
  }

  // If the client requests disabling, bypassing, or validating cache, don't
  // return a prefetch.
  // These are used mostly for reloads and dev tools.
  if (tentative_resource_request.load_flags & net::LOAD_BYPASS_CACHE ||
      tentative_resource_request.load_flags & net::LOAD_DISABLE_CACHE ||
      tentative_resource_request.load_flags & net::LOAD_VALIDATE_CACHE) {
    recorder.reason_ = SearchPrefetchServingReason::kPostReloadOrLink;
    return nullptr;
  }

  // Link clicks should not be served with a prefetch due to results page nth
  // page matching the URL pattern of the DSE.
  if (ui::PageTransitionCoreTypeIs(
          static_cast<ui::PageTransition>(
              tentative_resource_request.transition_type),
          ui::PAGE_TRANSITION_LINK)) {
    recorder.reason_ = SearchPrefetchServingReason::kPostReloadOrLink;
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

  DeletePrefetch(search_terms);

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
      profile_, BaseSearchPrefetchRequest::NetworkAnnotationForPrefetch(),
      prefetch_cache_[navigation_url_without_ref].first, nullptr);
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
    if (prefetch_request->current_status() != SearchPrefetchStatus::kInFlight &&
        prefetch_request->current_status() !=
            SearchPrefetchStatus::kCanBeServed) {
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
  }

  for (const auto& match : result) {
    if (BaseSearchProvider::ShouldPrefetch(match)) {
      MaybePrefetchURL(GetPrefetchURLFromMatch(match, template_url_service));
    }
    if (prerender_utils::IsSearchSuggestionPrerenderEnabled() &&
        BaseSearchProvider::ShouldPrerender(match) && web_contents) {
      PrerenderManager::CreateForWebContents(web_contents);
      auto* prerender_manager = PrerenderManager::FromWebContents(web_contents);
      prerender_manager->StartPrerenderSearchSuggestion(match);
    }
  }
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
  const base::Value* dictionary =
      profile_->GetPrefs()->GetDictionary(prefetch::prefs::kCachePrefPath);
  DCHECK(dictionary);

  auto* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile_);
  if (!template_url_service ||
      !template_url_service->GetDefaultSearchProvider()) {
    return dictionary->DictSize() > 0;
  }

  for (auto element : dictionary->DictItems()) {
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
  return dictionary->DictSize() > prefetch_cache_.size();
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
