// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefetch/search_prefetch/search_prefetch_service.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/net/prediction_options.h"
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
#include "components/prefs/pref_service.h"
#include "components/search_engines/template_url_service.h"
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

}  // namespace

SearchPrefetchService::SearchPrefetchService(Profile* profile)
    : profile_(profile) {
  DCHECK(!profile_->IsOffTheRecord());
}

SearchPrefetchService::~SearchPrefetchService() = default;

void SearchPrefetchService::Shutdown() {
  observer_.Reset();
}

bool SearchPrefetchService::MaybePrefetchURL(const GURL& url) {
  if (!SearchPrefetchServicePrefetchingIsEnabled())
    return false;

  if (!chrome_browser_net::CanPreresolveAndPreconnectUI(profile_->GetPrefs())) {
    return false;
  }

  if (!profile_->GetPrefs() ||
      !profile_->GetPrefs()->GetBoolean(prefs::kWebKitJavascriptEnabled)) {
    return false;
  }

  auto* content_settings =
      HostContentSettingsMapFactory::GetForProfile(profile_);
  if (!content_settings ||
      content_settings->GetContentSetting(
          url, url, ContentSettingsType::JAVASCRIPT) == CONTENT_SETTING_BLOCK) {
    return false;
  }

  auto* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile_);
  if (!template_url_service ||
      !template_url_service->GetDefaultSearchProvider())
    return false;

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

  base::string16 search_terms;

  // Extract the terms directly to make sure this string will match the URL
  // interception string logic.
  template_url_service->GetDefaultSearchProvider()->ExtractSearchTermsFromURL(
      url, template_url_service->search_terms_data(), &search_terms);

  if (search_terms.size() == 0)
    return false;

  if (last_error_time_ticks_ + SearchPrefetchErrorBackoffDuration() >
      base::TimeTicks::Now()) {
    return false;
  }

  if (prefetches_.size() >= SearchPrefetchMaxAttemptsPerCachingDuration())
    return false;

  // Don't prefetch the same search terms twice within the expiry duration.
  if (prefetches_.find(search_terms) != prefetches_.end()) {
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

  base::string16 match_search_terms;

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
    base::string16 search_terms) {
  if (prefetches_.find(search_terms) == prefetches_.end())
    return base::nullopt;
  return prefetches_[search_terms]->current_status();
}

std::unique_ptr<SearchPrefetchURLLoader>
SearchPrefetchService::TakePrefetchResponse(const GURL& url) {
  auto* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile_);
  if (!template_url_service ||
      !template_url_service->GetDefaultSearchProvider())
    return nullptr;

  // The user may have disabled JS since the prefetch occured.
  if (!profile_->GetPrefs() ||
      !profile_->GetPrefs()->GetBoolean(prefs::kWebKitJavascriptEnabled)) {
    return nullptr;
  }

  auto* content_settings =
      HostContentSettingsMapFactory::GetForProfile(profile_);
  if (!content_settings ||
      content_settings->GetContentSetting(
          url, url, ContentSettingsType::JAVASCRIPT) == CONTENT_SETTING_BLOCK) {
    return nullptr;
  }

  base::string16 search_terms;
  template_url_service->GetDefaultSearchProvider()->ExtractSearchTermsFromURL(
      url, template_url_service->search_terms_data(), &search_terms);

  if (search_terms.length() == 0) {
    return nullptr;
  }

  const auto& iter = prefetches_.find(search_terms);

  if (iter == prefetches_.end()) {
    return nullptr;
  }

  // Verify that the URL is the same origin as the prefetch URL. While other
  // checks should address this by clearing prefetches on user changes to
  // default search, it is paramount to never serve content from one origin to
  // another.
  if (url::Origin::Create(url) !=
      url::Origin::Create(iter->second->prefetch_url())) {
    return nullptr;
  }

  if (iter->second->current_status() != SearchPrefetchStatus::kComplete &&
      iter->second->current_status() !=
          SearchPrefetchStatus::kCanBeServedAndUserClicked) {
    return nullptr;
  }

  std::unique_ptr<SearchPrefetchURLLoader> response =
      iter->second->TakeSearchPrefetchURLLoader();

  // TODO(ryansturm): For metrics reporting, the prefetch request data should be
  // moved to the correct tab helper object, for now, the object can be deleted
  // entirely. Alternatively, the object can remain here with a new timeout in
  // a set of currently being served requests.
  DeletePrefetch(search_terms);

  return response;
}

void SearchPrefetchService::ClearPrefetches() {
  prefetches_.clear();
  prefetch_expiry_timers_.clear();
}

void SearchPrefetchService::DeletePrefetch(base::string16 search_terms) {
  DCHECK(prefetches_.find(search_terms) != prefetches_.end());
  DCHECK(prefetch_expiry_timers_.find(search_terms) !=
         prefetch_expiry_timers_.end());

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
        base::string16 match_search_terms;
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
