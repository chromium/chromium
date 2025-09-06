// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/search_preload/search_preload_pipeline.h"

#include "chrome/browser/preloading/chrome_preloading.h"
#include "chrome/browser/preloading/prefetch/search_prefetch/search_prefetch_service.h"
#include "chrome/browser/preloading/prerender/prerender_utils.h"
#include "chrome/browser/preloading/search_preload/search_preload_features.h"
#include "chrome/browser/preloading/search_preload/search_preload_service.h"
#include "content/public/browser/preloading_data.h"
#include "content/public/browser/preloading_trigger_type.h"
#include "content/public/browser/web_contents.h"
#include "net/http/http_no_vary_search_data.h"
#include "third_party/blink/public/mojom/loader/referrer.mojom.h"

SearchPreloadPipeline::SearchPreloadPipeline(GURL canonical_url)
    : pipeline_info_(content::PreloadPipelineInfo::Create(
          /*planned_max_preloading_type=*/content::PreloadingType::kPrerender)),
      canonical_url_(std::move(canonical_url)) {}

SearchPreloadPipeline::~SearchPreloadPipeline() = default;

void SearchPreloadPipeline::UpdateConfidence(content::WebContents& web_contents,
                                             int confidence) {
  // Add new prediction when stronger signal arrived.

  if (confidence <= confidence_) {
    return;
  }

  confidence_ = confidence;

  auto* preloading_data =
      content::PreloadingData::GetOrCreateForWebContents(&web_contents);

  // Safety: The ownership of this callback will be passed to
  // `PreloadingDataImpl`, which has lifetime bounded by `web_contents`. So,
  // it's safe to bind `content::BrowserContext*`.
  content::PreloadingURLMatchCallback same_url_matcher =
      base::BindRepeating(&IsSearchDestinationMatch, canonical_url_,
                          web_contents.GetBrowserContext());
  preloading_data->AddPreloadingPrediction(
      chrome_preloading_predictor::kDefaultSearchEngine, confidence,
      std::move(same_url_matcher),
      web_contents.GetPrimaryMainFrame()->GetPageUkmSourceId());
}

SearchPreloadSignalResult SearchPreloadPipeline::StartPrefetch(
    content::WebContents& web_contents,
    base::WeakPtr<SearchPreloadService> search_preload_service,
    const GURL& prefetch_url,
    content::PreloadingPredictor predictor,
    const std::optional<net::HttpNoVarySearchData>& no_vary_search_hint,
    bool is_navigation_likely) {
  // Don't trigger prefetch if already triggered and is alive.
  //
  // TODO(crbug.com/394213503): Reconsider the behavior when prefetch is already
  // triggered but not alive. Currently, the main reason that a triggered
  // prefetch fails for DSE (embedder trigger, no TTL) is the failure of the
  // load of the prefetch. (There should be no other timeouts nor expiration.)
  // In general, retriggering may be useful.
  if (prefetch_handle_) {
    return SearchPreloadSignalResult::kNotTriggeredAlreadyTriggered;
  }

  auto* preloading_data =
      content::PreloadingData::GetOrCreateForWebContents(&web_contents);

  // Safety: The ownership of this callback will be passed to
  // `PreloadingDataImpl`, which has lifetime bounded by `web_contents`. So,
  // it's safe to bind `content::BrowserContext*`.
  content::PreloadingURLMatchCallback same_url_matcher =
      base::BindRepeating(&IsSearchDestinationMatch, canonical_url_,
                          web_contents.GetBrowserContext());
  content::PreloadingAttempt* attempt = preloading_data->AddPreloadingAttempt(
      predictor, content::PreloadingType::kPrefetch,
      std::move(same_url_matcher),
      /*triggering_primary_page_source_id=*/
      web_contents.GetPrimaryMainFrame()->GetPageUkmSourceId());

  // TODO(crbug.com/379140429): Create `preloading_utils` and move common
  // preloading histograms suffixes to it.
  prefetch_handle_ = web_contents.StartPrefetch(
      prefetch_url,
      /*use_prefetch_proxy=*/false,
      prerender_utils::kDefaultSearchEngineMetricSuffix,
      blink::mojom::Referrer(),
      /*referring_origin=*/std::nullopt, no_vary_search_hint,
      /*priority=*/std::nullopt, pipeline_info_, attempt->GetWeakPtr(),
      /*holdback_status_override=*/
      content::PreloadingHoldbackStatus::kUnspecified,
      /*ttl=*/features::kDsePreload2PrefetchTtl.Get());
  CHECK(prefetch_handle_);
  prefetch_handle_->SetOnPrefetchHeadReceivedCallback(base::BindRepeating(
      &SearchPreloadService::OnPrefetchHeadReceived, search_preload_service));
  if (!is_navigation_likely) {
    prefetch_handle_->SetOnPrefetchCompletedOrFailedCallback(
        base::BindRepeating(
            &SearchPreloadService::OnSuggestPrefetchCompletedOrFailed,
            search_preload_service));
  }

  return SearchPreloadSignalResult::kPrefetchTriggered;
}

SearchPreloadSignalResult SearchPreloadPipeline::StartPrerender(
    content::WebContents& web_contents,
    const GURL& prerender_url,
    content::PreloadingPredictor predictor) {
  // Don't trigger prerender if already triggered.
  if (prerender_handle_) {
    return SearchPreloadSignalResult::kNotTriggeredAlreadyTriggered;
  }

  // Assume that prefetch is alive.
  if (!IsPrefetchAlive()) {
    return SearchPreloadSignalResult::kNotTriggeredPrefetchNotAlive;
  }

  auto* preloading_data =
      content::PreloadingData::GetOrCreateForWebContents(&web_contents);

  // Safety: The ownership of this callback will be passed to
  // `PreloadingDataImpl`, which has lifetime bounded by `web_contents`. So,
  // it's safe to bind `content::BrowserContext*`.
  content::PreloadingURLMatchCallback same_url_matcher =
      base::BindRepeating(&IsSearchDestinationMatch, canonical_url_,
                          web_contents.GetBrowserContext());
  content::PreloadingAttempt* attempt = preloading_data->AddPreloadingAttempt(
      predictor, content::PreloadingType::kPrerender,
      std::move(same_url_matcher),
      /*triggering_primary_page_source_id=*/
      web_contents.GetPrimaryMainFrame()->GetPageUkmSourceId());

  // Safety: The ownership of this callback will be passed to
  // `PrerenderAttributes`, which has lifetime bounded by `web_contents`. So,
  // it's safe to bind `content::BrowserContext*`.
  base::RepeatingCallback<bool(const GURL&,
                               const std::optional<content::UrlMatchType>&)>
      url_match_predicate =
          base::BindRepeating(&IsSearchDestinationMatchWithWebUrlMatchResult,
                              canonical_url_, web_contents.GetBrowserContext());

  prerender_handle_ = web_contents.StartPrerendering(
      prerender_url, content::PreloadingTriggerType::kEmbedder,
      prerender_utils::kDefaultSearchEngineMetricSuffix,
      /*additional_headers=*/net::HttpRequestHeaders(),
      /*no_vary_search_hint=*/std::nullopt,
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_GENERATED |
                                ui::PAGE_TRANSITION_FROM_ADDRESS_BAR),
      /*should_warm_up_compositor=*/true,
      /*should_prepare_paint_tree=*/true,
      content::PreloadingHoldbackStatus::kUnspecified, pipeline_info_, attempt,
      std::move(url_match_predicate),
      /*prerender_navigation_handle_callback=*/{},
      /*allow_reuse=*/false);
  return SearchPreloadSignalResult::kPrerenderTriggered;
}

void SearchPreloadPipeline::CancelPrerender() {
  prerender_handle_.reset();
}

bool SearchPreloadPipeline::IsPrefetchAlive() const {
  return prefetch_handle_ && prefetch_handle_->IsAlive();
}

bool SearchPreloadPipeline::IsPrerenderValid() const {
  return prerender_handle_ && prerender_handle_->IsValid();
}
