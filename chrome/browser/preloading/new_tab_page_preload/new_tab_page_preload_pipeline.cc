// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/new_tab_page_preload/new_tab_page_preload_pipeline.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/preloading/chrome_preloading.h"
#include "chrome/browser/preloading/prerender/prerender_manager.h"
#include "chrome/browser/preloading/prerender/prerender_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/page_load_metrics/browser/navigation_handle_user_data.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/preloading_data.h"
#include "content/public/browser/preloading_trigger_type.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/loader/referrer.mojom.h"

namespace {

const char kHistogramPrerenderNTPIsPrerenderingSrpUrl[] =
    "Prerender.IsPrerenderingSRPUrl.Embedder_NewTabPage";

bool IsSearchUrl(content::WebContents& web_contents, const GURL& url) {
  auto* profile = Profile::FromBrowserContext(web_contents.GetBrowserContext());
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile);
  return template_url_service &&
         template_url_service->IsSearchResultsPageFromDefaultSearchProvider(
             url);
}

}  // namespace

NewTabPagePreloadPipeline::NewTabPagePreloadPipeline(GURL url)
    : pipeline_info_(content::PreloadPipelineInfo::Create(
          /*planned_max_preloading_type=*/content::PreloadingType::kPrerender)),
      url_(std::move(url)) {}

NewTabPagePreloadPipeline::~NewTabPagePreloadPipeline() = default;

bool NewTabPagePreloadPipeline::StartPrerender(
    content::WebContents& web_contents,
    content::PreloadingPredictor predictor) {
  if (prerender_handle_ && prerender_handle_->IsValid()) {
    return true;
  }

  // Helpers to create content::PreloadingAttempt.
  auto* preloading_data =
      content::PreloadingData::GetOrCreateForWebContents(&web_contents);
  content::PreloadingURLMatchCallback same_url_matcher =
      content::PreloadingData::GetSameURLMatcher(url_);
  content::PreloadingAttempt* preloading_attempt =
      preloading_data->AddPreloadingAttempt(
          predictor, content::PreloadingType::kPrerender,
          std::move(same_url_matcher),
          web_contents.GetPrimaryMainFrame()->GetPageUkmSourceId());

  bool is_search_url = IsSearchUrl(web_contents, url_);
  base::UmaHistogramBoolean(kHistogramPrerenderNTPIsPrerenderingSrpUrl,
                            is_search_url);
  if (is_search_url) {
    preloading_attempt->SetEligibility(ToPreloadingEligibility(
        ChromePreloadingEligibility::KDisallowSearchUrl));
    return false;
  }

  // NewTabPage only allows https protocol.
  if (!url_.SchemeIs("https")) {
    preloading_attempt->SetEligibility(
        content::PreloadingEligibility::kHttpsOnly);
    return false;
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

  return prerender_handle_ != nullptr;
}
