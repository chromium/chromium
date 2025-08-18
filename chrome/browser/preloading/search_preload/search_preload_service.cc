// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/search_preload/search_preload_service.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/preloading/search_preload/search_preload_features.h"
#include "chrome/browser/preloading/search_preload/search_preload_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/omnibox/browser/omnibox.mojom-shared.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/web_contents.h"
#include "services/network/public/mojom/no_vary_search.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace {

net::HttpNoVarySearchData ParseHttpNoVarySearchDataFromMojom(
    const network::mojom::NoVarySearchPtr& no_vary_search_ptr) {
  if (no_vary_search_ptr->search_variance->is_vary_params()) {
    return net::HttpNoVarySearchData::CreateFromVaryParams(
        no_vary_search_ptr->search_variance->get_vary_params(),
        no_vary_search_ptr->vary_on_key_order);
  }
  return net::HttpNoVarySearchData::CreateFromNoVaryParams(
      no_vary_search_ptr->search_variance->get_no_vary_params(),
      no_vary_search_ptr->vary_on_key_order);
}

}  // namespace

// static
SearchPreloadService* SearchPreloadService::GetForProfile(Profile* profile) {
  return SearchPreloadServiceFactory::GetForProfile(profile);
}

SearchPreloadService::SearchPreloadService(Profile* profile)
    : profile_(profile) {
  CHECK(features::IsDsePreload2Enabled());

  auto* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile_);
  CHECK(template_url_service);
  observer_.Observe(template_url_service);
}

SearchPreloadService::~SearchPreloadService() = default;

void SearchPreloadService::Shutdown() {
  ClearPreloads();
  observer_.Reset();
}

void SearchPreloadService::OnTemplateURLServiceChanged() {
  ClearPreloads();
}

void SearchPreloadService::ClearPreloads() {
  if (pipeline_manager_.has_value() && pipeline_manager_.value()) {
    pipeline_manager_.value()->ClearPreloads();
  }
  pipeline_manager_.reset();
}

void SearchPreloadService::OnPrefetchHeadReceived(
    const network::mojom::URLResponseHead& head) {
  auto no_vary_search_header =
      [&]() -> std::optional<net::HttpNoVarySearchData> {
    // No No-Vary-Search headers
    if (!(head.parsed_headers &&
          head.parsed_headers->no_vary_search_with_parse_error)) {
      return std::nullopt;
    }

    // Error
    if (head.parsed_headers->no_vary_search_with_parse_error
            ->is_parse_error()) {
      return std::nullopt;
    }

    // Success
    return ParseHttpNoVarySearchDataFromMojom(
        head.parsed_headers->no_vary_search_with_parse_error
            ->get_no_vary_search());
  }();

  {
    SearchPreloadServiceNoVarySearchDataCacheUpdate update;
    if (no_vary_search_data_cache_ == no_vary_search_header) {
      update = SearchPreloadServiceNoVarySearchDataCacheUpdate::kUnchanged;
    } else {
      const bool had_value = no_vary_search_data_cache_.has_value();
      const bool has_value = no_vary_search_header.has_value();
      if (had_value && has_value) {
        update = SearchPreloadServiceNoVarySearchDataCacheUpdate::kSomeToSome;
      } else if (!had_value && has_value) {
        update = SearchPreloadServiceNoVarySearchDataCacheUpdate::kNullToSome;
      } else if (had_value && !has_value) {
        update = SearchPreloadServiceNoVarySearchDataCacheUpdate::kSomeToNull;
      } else {
        NOTREACHED();
      }
    }
    base::UmaHistogramEnumeration(
        "Omnibox.DsePreload.Prefetch.NoVarySearchDataCacheUpdate", update);
  }

  // TODO(crbug.com/422074579): Persist it to profile.
  if (no_vary_search_data_cache_ != no_vary_search_header) {
    no_vary_search_data_cache_ = std::move(no_vary_search_header);
  }
}

void SearchPreloadService::OnSuggestPrefetchCompletedOrFailed(
    const network::URLLoaderCompletionStatus& completion_status,
    const std::optional<int>& response_code) {
  const bool is_2xx = response_code.has_value() &&
                      (200 <= response_code && response_code < 300);
  if (!is_2xx) {
    pause_triggering_until_ = base::TimeTicks::Now() +
                              features::kDsePreload2ErrorBackoffDuration.Get();
  }
}

SearchPreloadPipelineManager&
SearchPreloadService::GetOrCreatePipelineManagerWithLimit(
    content::WebContents& web_contents) {
  // Allow at most one WebContents to hold preloads.
  //
  // TODO(crbug.com/394213503): Reconsider the limitation.
  const bool is_occupied_with_given_web_contents =
      pipeline_manager_.has_value() && pipeline_manager_.value() &&
      &pipeline_manager_.value()->GetWebContents() == &web_contents;
  if (!is_occupied_with_given_web_contents) {
    ClearPreloads();
  }

  if (!pipeline_manager_.has_value()) {
    SearchPreloadPipelineManager::CreateForWebContents(&web_contents);
    auto* pipeline_manager =
        SearchPreloadPipelineManager::FromWebContents(&web_contents);
    CHECK(pipeline_manager);
    pipeline_manager_ = pipeline_manager->GetWeakPtr();
  }

  return *pipeline_manager_.value();
}

void SearchPreloadService::OnAutocompleteResultChanged(
    content::WebContents* web_contents,
    const AutocompleteResult& result) {
  if (!web_contents) {
    return;
  }

  if (base::TimeTicks::Now() < pause_triggering_until_) {
    return;
  }

  GetOrCreatePipelineManagerWithLimit(*web_contents)
      .OnAutocompleteResultChanged(*profile_, GetWeakPtr(), result,
                                   no_vary_search_data_cache_);
}

bool SearchPreloadService::OnNavigationLikely(
    size_t index,
    const AutocompleteMatch& match,
    omnibox::mojom::NavigationPredictor navigation_predictor,
    content::WebContents* web_contents) {
  if (!web_contents) {
    return false;
  }

  if (base::TimeTicks::Now() < pause_triggering_until_) {
    return false;
  }

  return GetOrCreatePipelineManagerWithLimit(*web_contents)
      .OnNavigationLikely(*profile_, GetWeakPtr(), match, navigation_predictor,
                          no_vary_search_data_cache_);
}

const std::optional<net::HttpNoVarySearchData>&
SearchPreloadService::GetNoVarySearchDataCacheForTesting() const {
  return no_vary_search_data_cache_;
}

void SearchPreloadService::SetNoVarySearchDataCacheForTesting(
    std::optional<net::HttpNoVarySearchData> no_vary_search_data) {
  no_vary_search_data_cache_ = std::move(no_vary_search_data);
}

bool SearchPreloadService::InvalidatePipelineForTesting(
    content::WebContents& web_contents,
    GURL canonical_url) {
  return GetOrCreatePipelineManagerWithLimit(web_contents)
      .InvalidatePipelineForTesting(canonical_url);  // IN-TEST
}
