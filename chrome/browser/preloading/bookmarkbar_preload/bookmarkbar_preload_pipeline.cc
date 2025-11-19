// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/bookmarkbar_preload/bookmarkbar_preload_pipeline.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/preloading/chrome_preloading.h"
#include "chrome/browser/preloading/preloading_features.h"
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

constexpr char kHistogramPrerenderBookmarkBarIsPrerenderingSrpUrl[] =
    "Prerender.IsPrerenderingSRPUrl.Embedder_BookmarkBar";

// TODO(crbug.com/413259638): Create `preloading_utils` and move this to it.
constexpr char kBookmarkBarMetricSuffix[] = "BookmarkBar";

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
  return (template_url_service &&
          template_url_service->IsSearchResultsPageFromDefaultSearchProvider(
              url)) ||
         google_util::IsGoogleSearchUrl(url);
}

}  // namespace

BookmarkBarPreloadPipeline::BookmarkBarPreloadPipeline(GURL url)
    : pipeline_info_(content::PreloadPipelineInfo::Create(
          /*planned_max_preloading_type=*/content::PreloadingType::kPrerender)),
      url_(std::move(url)) {}

BookmarkBarPreloadPipeline::~BookmarkBarPreloadPipeline() = default;

void BookmarkBarPreloadPipeline::StartPrefetch(
    content::WebContents& web_contents,
    content::PreloadingPredictor predictor) {
  // Don't trigger prefetch if already triggered.
  if (prefetch_handle_) {
    return;
  }

  auto* preloading_data =
      content::PreloadingData::GetOrCreateForWebContents(&web_contents);

  content::PreloadingURLMatchCallback same_url_matcher =
      content::PreloadingData::GetSameURLMatcher(url_);
  content::PreloadingAttempt* attempt = preloading_data->AddPreloadingAttempt(
      predictor, content::PreloadingType::kPrefetch,
      std::move(same_url_matcher),
      web_contents.GetPrimaryMainFrame()->GetPageUkmSourceId());

  bool is_search_url = IsSearchUrl(web_contents, url_);
  base::UmaHistogramBoolean(
      "Navigation.Prefetch.IsPrefetchingSRPUrl.Embedder_BookmarkBar",
      is_search_url);

  if (is_search_url) {
    attempt->SetEligibility(ToPreloadingEligibility(
        ChromePreloadingEligibility::KDisallowSearchUrl));
    return;
  }

  prefetch_handle_ = web_contents.StartPrefetch(
      url_, /*use_prefetch_proxy=*/false, kBookmarkBarMetricSuffix,
      blink::mojom::Referrer(), /*referring_origin=*/std::nullopt,
      /*no_vary_search_hint=*/std::nullopt, /*priority=*/std::nullopt,
      pipeline_info_, attempt->GetWeakPtr(),
      /*holdback_status_override=*/
      content::PreloadingHoldbackStatus::kUnspecified, /*ttl=*/std::nullopt);
}

void BookmarkBarPreloadPipeline::StartPrerender(
    content::WebContents& web_contents,
    content::PreloadingPredictor predictor) {
  if (base::FeatureList::IsEnabled(
          features::kBookmarkTriggerForPrerender2KillSwitch)) {
    return;
  }

  // Don't trigger prerender if already triggered.
  if (prerender_handle_) {
    return;
  }
  // Helpers to create content::PreloadingAttempt.
  auto* preloading_data =
      content::PreloadingData::GetOrCreateForWebContents(&web_contents);
  content::PreloadingURLMatchCallback same_url_matcher =
      content::PreloadingData::GetSameURLMatcher(url_);

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
    return;
  }

  // Currently, this method has a precondition: It should be called after
  // `StartPrefetch()` if the feature is enabled. The precondition was checked
  // at the head of the method, but we found that `StartPrefetch()` might not
  // trigger prefetch as prefetch could fail due to some eligibility checks
  // which prerender hadn't executed yet before the CHECK. For more detalis, see
  // https://crbug.com/449105853
  //
  // So, we place the check here, after the same eligibility check as in the
  // `StartPrefetch()`.
  //
  // TODO(crbug.com/413259638): Remove this CHECK when a refactor to
  // `BookmarkBarPreloadPipelineManager` is done to guarantee the order of
  // prefetch ahead prerender. For more details of the refactor goal, please see
  // the comments in `BookmarkBarPreloadPipelineManager`.
  CHECK(!base::FeatureList::IsEnabled(features::kBookmarkTriggerForPrefetch) ||
        prefetch_handle_);

  // BookmarkBar only allows https protocol.
  if (!url_.SchemeIs("https")) {
    preloading_attempt->SetEligibility(
        content::PreloadingEligibility::kHttpsOnly);
    return;
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
      std::move(prerender_navigation_handle_callback),
      /*allow_reuse=*/false);
}

void BookmarkBarPreloadPipeline::
    SetOnPrefetchCompletedOrFailedCallbackForTesting(
        base::RepeatingCallback<
            void(const network::URLLoaderCompletionStatus& completion_status,
                 const std::optional<int>& response_code)>
            on_prefetch_completed_or_failed) {
  CHECK(prefetch_handle_);
  prefetch_handle_->SetOnPrefetchCompletedOrFailedCallback(
      std::move(on_prefetch_completed_or_failed));
}
