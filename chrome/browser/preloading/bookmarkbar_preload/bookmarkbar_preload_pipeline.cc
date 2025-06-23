// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/bookmarkbar_preload/bookmarkbar_preload_pipeline.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/preloading/chrome_preloading.h"
#include "chrome/browser/preloading/preloading_features.h"
#include "chrome/browser/preloading/prerender/prerender_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/page_load_metrics/browser/navigation_handle_user_data.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/preloading_data.h"
#include "content/public/browser/preloading_trigger_type.h"
#include "content/public/browser/web_contents.h"

namespace {

const char kHistogramPrerenderBookmarkBarIsPrerenderingSrpUrl[] =
    "Prerender.IsPrerenderingSRPUrl.Embedder_BookmarkBar";

void AttachBookmarkBarNavigationHandleUserData(
    content::NavigationHandle& navigation_handle) {
  page_load_metrics::NavigationHandleUserData::CreateForNavigationHandle(
      navigation_handle, page_load_metrics::NavigationHandleUserData::
                             InitiatorLocation::kBookmarkBar);
}

bool IsSearchUrl(content::WebContents& web_contents, const GURL& url) {
  auto* profile = Profile::FromBrowserContext(web_contents.GetBrowserContext());
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile);
  return template_url_service &&
         template_url_service->IsSearchResultsPageFromDefaultSearchProvider(
             url);
}

}  // namespace

BookmarkBarPreloadPipeline::BookmarkBarPreloadPipeline(GURL url)
    : pipeline_info_(content::PreloadPipelineInfo::Create(
          /*planned_max_preloading_type=*/content::PreloadingType::kPrerender)),
      url_(std::move(url)) {}

BookmarkBarPreloadPipeline::~BookmarkBarPreloadPipeline() = default;

bool BookmarkBarPreloadPipeline::StartPrerender(
    content::WebContents& web_contents,
    content::PreloadingPredictor predictor) {
  // Helpers to create content::PreloadingAttempt.
  auto* preloading_data =
      content::PreloadingData::GetOrCreateForWebContents(&web_contents);
  content::PreloadingURLMatchCallback same_url_matcher =
      content::PreloadingData::GetSameURLMatcher(url_);

  if (prerender_handle_ && prerender_handle_->IsValid()) {
    return true;
  }

  bool is_search_url = IsSearchUrl(web_contents, url_);
  base::UmaHistogramBoolean(kHistogramPrerenderBookmarkBarIsPrerenderingSrpUrl,
                            is_search_url);

  // Create new PreloadingAttempt and pass all the values corresponding to
  // this prerendering attempt for Prerender.
  content::PreloadingAttempt* preloading_attempt =
      preloading_data->AddPreloadingAttempt(
          predictor, content::PreloadingType::kPrerender,
          std::move(same_url_matcher),
          web_contents.GetPrimaryMainFrame()->GetPageUkmSourceId());
  if (is_search_url) {
    preloading_attempt->SetEligibility(ToPreloadingEligibility(
        ChromePreloadingEligibility::KDisallowSearchUrl));
    return false;
  }

  // BookmarkBar only allows https protocol.
  if (!url_.SchemeIs("https")) {
    preloading_attempt->SetEligibility(
        content::PreloadingEligibility::kHttpsOnly);
    return false;
  }

  base::RepeatingCallback<void(content::NavigationHandle&)>
      prerender_navigation_handle_callback =
          base::BindRepeating(&AttachBookmarkBarNavigationHandleUserData);

  prerender_handle_ = web_contents.StartPrerendering(
      url_, content::PreloadingTriggerType::kEmbedder,
      prerender_utils::kBookmarkBarMetricSuffix,
      /*additional_headers=*/net::HttpRequestHeaders(),
      /*no_vary_search_hint=*/std::nullopt,
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_AUTO_BOOKMARK),
      /*should_warm_up_compositor=*/
      base::FeatureList::IsEnabled(
          features::kPrerender2WarmUpCompositorForBookmarkBar),
      /*should_prepare_paint_tree=*/false,
      content::PreloadingHoldbackStatus::kUnspecified, pipeline_info_,
      preloading_attempt,
      /*url_match_predicate=*/{},
      std::move(prerender_navigation_handle_callback));
  return prerender_handle_ != nullptr;
}
