// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/new_tab_page_preload/new_tab_page_preload_pipeline.h"

#include "base/debug/dump_without_crashing.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/preloading/chrome_preloading.h"
#include "chrome/browser/preloading/prerender/prerender_manager.h"
#include "chrome/browser/preloading/prerender/prerender_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/google/core/common/google_util.h"
#include "components/page_load_metrics/browser/navigation_handle_user_data.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/preloading_data.h"
#include "content/public/browser/preloading_trigger_type.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/loader/referrer.mojom.h"

namespace {

constexpr char kHistogramPrerenderNTPIsPrerenderingSrpUrl[] =
    "Prerender.IsPrerenderingSRPUrl.Embedder_NewTabPage";

// TODO(crbug.com/413259638): Create `preloading_utils` and move this to it.
constexpr char kNewTabPageMetricSuffix[] = "NewTabPage";

bool IsSearchUrl(content::WebContents& web_contents, const GURL& url) {
  auto* profile = Profile::FromBrowserContext(web_contents.GetBrowserContext());
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile);
  return (template_url_service &&
          template_url_service->IsSearchResultsPageFromDefaultSearchProvider(
              url)) ||
         google_util::IsGoogleSearchUrl(url);
}

}  // namespace

NewTabPagePreloadPipeline::NewTabPagePreloadPipeline(GURL url)
    : pipeline_info_(content::PreloadPipelineInfo::Create(
          /*planned_max_preloading_type=*/content::PreloadingType::kPrerender)),
      url_(std::move(url)) {}

NewTabPagePreloadPipeline::~NewTabPagePreloadPipeline() = default;

void NewTabPagePreloadPipeline::StartPrefetch(
    content::WebContents& web_contents,
    content::PreloadingPredictor predictor) {
  CHECK(base::FeatureList::IsEnabled(features::kNewTabPageTriggerForPrefetch));
  // Don't trigger prefetch if already triggered.
  if (prefetch_handle_) {
    return;
  }

  if (prerender_handle_) {
    // a prerender handle isn't expected to exist before a prefetch request.
    base::debug::DumpWithoutCrashing();
    return;
  }

  auto* preloading_data =
      content::PreloadingData::GetOrCreateForWebContents(&web_contents);
  content::PreloadingAttempt* attempt = preloading_data->AddPreloadingAttempt(
      predictor, content::PreloadingType::kPrefetch,
      content::PreloadingData::GetSameURLMatcher(url_),
      web_contents.GetPrimaryMainFrame()->GetPageUkmSourceId());

  if (IsSearchUrl(web_contents, url_)) {
    attempt->SetEligibility(ToPreloadingEligibility(
        ChromePreloadingEligibility::KDisallowSearchUrl));
    return;
  }

  prefetch_handle_ = web_contents.StartPrefetch(
      url_, /*use_prefetch_proxy=*/false, kNewTabPageMetricSuffix,
      blink::mojom::Referrer(), /*referring_origin=*/std::nullopt,
      /*no_vary_search_hint=*/std::nullopt, /*priority=*/std::nullopt,
      pipeline_info_, attempt->GetWeakPtr(),
      /*holdback_status_override=*/
      content::PreloadingHoldbackStatus::kUnspecified, /*ttl=*/std::nullopt);
}

void NewTabPagePreloadPipeline::StartPrerender(
    content::WebContents& web_contents,
    content::PreloadingPredictor predictor) {
  if (prerender_handle_) {
    return;
  }

  // Helpers to create content::PreloadingAttempt.
  auto* preloading_data =
      content::PreloadingData::GetOrCreateForWebContents(&web_contents);
  content::PreloadingAttempt* preloading_attempt =
      preloading_data->AddPreloadingAttempt(
          predictor, content::PreloadingType::kPrerender,
          content::PreloadingData::GetSameURLMatcher(url_),
          web_contents.GetPrimaryMainFrame()->GetPageUkmSourceId());

  bool is_search_url = IsSearchUrl(web_contents, url_);
  base::UmaHistogramBoolean(kHistogramPrerenderNTPIsPrerenderingSrpUrl,
                            is_search_url);
  if (is_search_url) {
    preloading_attempt->SetEligibility(ToPreloadingEligibility(
        ChromePreloadingEligibility::KDisallowSearchUrl));
    return;
  }

  // NewTabPage only allows https protocol.
  if (!url_.SchemeIs("https")) {
    preloading_attempt->SetEligibility(
        content::PreloadingEligibility::kHttpsOnly);
    return;
  }

  prerender_handle_ = web_contents.StartPrerendering(
      url_, content::PreloadingTriggerType::kEmbedder,
      prerender_utils::kNewTabPageMetricSuffix,
      /*additional_headers=*/net::HttpRequestHeaders(),
      /*no_vary_search_hint=*/std::nullopt,
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_AUTO_BOOKMARK),
      /*should_warm_up_compositor=*/
      base::FeatureList::IsEnabled(
          features::kPrerender2WarmUpCompositorForNewTabPage),
      /*should_prepare_paint_tree=*/false,
      content::PreloadingHoldbackStatus::kUnspecified, pipeline_info_,
      preloading_attempt,
      /*url_match_predicate=*/{},
      base::BindRepeating(&page_load_metrics::NavigationHandleUserData::
                              AttachNewTabPageNavigationHandleUserData),
      /*allow_reuse=*/false);
}
