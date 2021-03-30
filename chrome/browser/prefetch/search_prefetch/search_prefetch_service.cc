// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefetch/search_prefetch/search_prefetch_service.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/util/values/values_util.h"
#include "base/values.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/net/prediction_options.h"
#include "chrome/browser/prefetch/pref_names.h"
#include "chrome/browser/prefetch/search_prefetch/back_forward_search_prefetch_url_loader.h"
#include "chrome/browser/prefetch/search_prefetch/field_trial_settings.h"
#include "chrome/browser/prefetch/search_prefetch/full_body_search_prefetch_request.h"
#include "chrome/browser/prefetch/search_prefetch/search_prefetch_url_loader.h"
#include "chrome/browser/prefetch/search_prefetch/streaming_search_prefetch_request.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/search_engines/ui_thread_search_terms_data.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/base_search_provider.h"
#include "components/omnibox/browser/omnibox_event_global_tracker.h"
#include "components/omnibox/browser/omnibox_log.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/template_url_service.h"
#include "net/base/load_flags.h"
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
  SearchPrefetchEligibilityReasonRecorder() = default;
  ~SearchPrefetchEligibilityReasonRecorder() {
    UMA_HISTOGRAM_ENUMERATION(
        "Omnibox.SearchPrefetch.PrefetchEligibilityReason", reason_);
  }

  SearchPrefetchEligibilityReason reason_ =
      SearchPrefetchEligibilityReason::kPrefetchStarted;
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

void RecordFinalStatus(SearchPrefetchStatus status) {
  UMA_HISTOGRAM_ENUMERATION("Omnibox.SearchPrefetch.PrefetchFinalStatus",
                            status);
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
  if (!SearchPrefetchServicePrefetchingIsEnabled())
    return false;

  SearchPrefetchEligibilityReasonRecorder recorder;

  if (!chrome_browser_net::CanPreresolveAndPreconnectUI(profile_->GetPrefs())) {
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
  if (!observer_.IsObserving()) {
    observer_.Observe(template_url_service);
    const TemplateURL* template_url =
        template_url_service->GetDefaultSearchProvider();
    if (template_url) {
      template_url_service_data_ = template_url->data();
    }

    omnibox_subscription_ =
        OmniboxEventGlobalTracker::GetInstance()->RegisterCallback(
            base::BindRepeating(&SearchPrefetchService::OnURLOpenedFromOmnibox,
                                base::Unretained(this)));
  }

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

  if (last_error_time_ticks_ + SearchPrefetchErrorBackoffDuration() >
      base::TimeTicks::Now()) {
    recorder.reason_ = SearchPrefetchEligibilityReason::kErrorBackoff;
    return false;
  }

  // Don't prefetch the same search terms twice within the expiry duration.
  if (prefetches_.find(search_terms) != prefetches_.end()) {
    recorder.reason_ = SearchPrefetchEligibilityReason::kAttemptedQueryRecently;
    return false;
  }

  if (prefetches_.size() >= SearchPrefetchMaxAttemptsPerCachingDuration()) {
    recorder.reason_ = SearchPrefetchEligibilityReason::kMaxAttemptsReached;
    return false;
  }

  std::unique_ptr<BaseSearchPrefetchRequest> prefetch_request = nullptr;
  if (StreamSearchPrefetchResponses()) {
    prefetch_request = std::make_unique<StreamingSearchPrefetchRequest>(
        url, base::BindOnce(&SearchPrefetchService::ReportError,
                            base::Unretained(this)));
  } else {
    prefetch_request = std::make_unique<FullBodySearchPrefetchRequest>(
        url, base::BindOnce(&SearchPrefetchService::ReportError,
                            base::Unretained(this)));
  }

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
  const AutocompleteMatch& match = log->result.match_at(log->selected_index);
  const GURL& opened_url = match.destination_url;

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

  if (prefetches_.find(match_search_terms) == prefetches_.end() ||
      prefetches_[match_search_terms]->current_status() !=
          SearchPrefetchStatus::kCanBeServed) {
    return;
  }
  prefetches_[match_search_terms]->MarkPrefetchAsClicked();
}

base::Optional<SearchPrefetchStatus>
SearchPrefetchService::GetSearchPrefetchStatusForTesting(
    std::u16string search_terms) {
  if (prefetches_.find(search_terms) == prefetches_.end())
    return base::nullopt;
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

  AddCacheEntry(navigation_url, iter->second->prefetch_url());

  DeletePrefetch(search_terms);

  return response;
}

std::unique_ptr<SearchPrefetchURLLoader>
SearchPrefetchService::TakePrefetchResponseFromDiskCache(
    const GURL& navigation_url) {
  if (prefetch_cache_.find(navigation_url) == prefetch_cache_.end()) {
    return nullptr;
  }

  return std::make_unique<BackForwardSearchPrefetchURLLoader>(
      profile_, BaseSearchPrefetchRequest::NetworkAnnotationForPrefetch(),
      prefetch_cache_[navigation_url].first);
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

  RecordFinalStatus(prefetches_[search_terms]->current_status());

  prefetches_.erase(search_terms);
  prefetch_expiry_timers_.erase(search_terms);
}

void SearchPrefetchService::ReportError() {
  last_error_time_ticks_ = base::TimeTicks::Now();
}

void SearchPrefetchService::OnResultChanged(
    AutocompleteController* controller) {
  const auto& result = controller->result();
  const auto* default_match = result.default_match();

  auto* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile_);
  DCHECK(template_url_service);
  auto* default_search = template_url_service->GetDefaultSearchProvider();
  if (!default_search)
    return;

  // Cancel Unneeded prefetch requests.
  if (SearchPrefetchShouldCancelUneededInflightRequests()) {
    // Since we limit the number of prefetches in the map, this should be fast
    // despite the two loops.
    for (const auto& kv_pair : prefetches_) {
      const auto& search_terms = kv_pair.first;
      auto& prefetch_request = kv_pair.second;
      if (prefetch_request->current_status() !=
              SearchPrefetchStatus::kInFlight &&
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
  }

  // One arm of the experiment only prefetches the top match when it is default.
  if (SearchPrefetchOnlyFetchDefaultMatch()) {
    if (default_match && BaseSearchProvider::ShouldPrefetch(*default_match)) {
      MaybePrefetchURL(
          GetPrefetchURLFromMatch(*default_match, template_url_service));
    }
    return;
  }

  for (const auto& match : result) {
    if (BaseSearchProvider::ShouldPrefetch(match)) {
      MaybePrefetchURL(GetPrefetchURLFromMatch(match, template_url_service));
    }
  }
}

void SearchPrefetchService::OnTemplateURLServiceChanged() {
  auto* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile_);
  DCHECK(template_url_service);

  base::Optional<TemplateURLData> template_url_service_data;

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
  if (prefetch_cache_.find(navigation_url) == prefetch_cache_.end()) {
    return;
  }

  prefetch_cache_.erase(navigation_url);
  SaveToPrefs();
}

void SearchPrefetchService::UpdateServeTime(const GURL& navigation_url) {
  if (prefetch_cache_.find(navigation_url) == prefetch_cache_.end())
    return;

  prefetch_cache_[navigation_url].second = base::Time::Now();
  SaveToPrefs();
}

void SearchPrefetchService::AddCacheEntry(const GURL& navigation_url,
                                          const GURL& prefetch_url) {
  if (navigation_url == prefetch_url) {
    return;
  }

  prefetch_cache_.emplace(navigation_url,
                          std::make_pair(prefetch_url, base::Time::Now()));

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
  const base::DictionaryValue* dictionary =
      profile_->GetPrefs()->GetDictionary(prefetch::prefs::kCachePrefPath);
  DCHECK(dictionary);

  auto* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile_);
  if (!template_url_service ||
      !template_url_service->GetDefaultSearchProvider()) {
    return dictionary->size() > 0;
  }

  for (const auto& element : *dictionary) {
    GURL navigation_url(element.first);
    if (!navigation_url.is_valid()) {
      continue;
    }

    if (!element.second) {
      continue;
    }

    base::Value::ConstListView const prefetch_url_and_time =
        base::Value::AsListValue(*element.second).GetList();

    if (prefetch_url_and_time.size() != 2 ||
        !prefetch_url_and_time[0].is_string() ||
        !prefetch_url_and_time[1].is_string()) {
      continue;
    }

    std::string prefetch_url;
    if (!prefetch_url_and_time[0].GetAsString(&prefetch_url)) {
      continue;
    }

    // Make sure we are only mapping same origin in case of corrupted prefs.
    if (url::Origin::Create(navigation_url) !=
        url::Origin::Create(GURL(prefetch_url))) {
      continue;
    }

    // Don't redirect same URL.
    if (navigation_url == prefetch_url) {
      continue;
    }

    // Make sure the navigation URL is still a search URL.
    std::u16string search_terms;
    template_url_service->GetDefaultSearchProvider()->ExtractSearchTermsFromURL(
        navigation_url, template_url_service->search_terms_data(),
        &search_terms);

    if (search_terms.size() == 0) {
      continue;
    }

    base::Optional<base::Time> last_update =
        util::ValueToTime(prefetch_url_and_time[1]);
    if (!last_update) {
      continue;
    }

    // This time isn't valid.
    if (last_update.value() > base::Time::Now()) {
      continue;
    }

    prefetch_cache_.emplace(
        navigation_url,
        std::make_pair(GURL(prefetch_url), last_update.value()));
  }
  return dictionary->size() > prefetch_cache_.size();
}

void SearchPrefetchService::SaveToPrefs() const {
  base::DictionaryValue dictionary;
  for (const auto& element : prefetch_cache_) {
    std::string navigation_url = element.first.spec();
    std::string prefetch_url = element.second.first.spec();
    auto time =
        std::make_unique<base::Value>(util::TimeToValue(element.second.second));
    base::ListValue value;
    value.AppendString(prefetch_url);
    value.Append(std::move(time));
    dictionary.SetKey(std::move(navigation_url), std::move(value));
  }
  profile_->GetPrefs()->Set(prefetch::prefs::kCachePrefPath, dictionary);
}

bool SearchPrefetchService::LoadFromPrefsForTesting() {
  return LoadFromPrefs();
}
