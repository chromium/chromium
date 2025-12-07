// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/prefetch/chrome_prefetch_manager.h"

#include "chrome/browser/preloading/chrome_preloading.h"
#include "content/public/browser/preload_pipeline_info.h"
#include "content/public/browser/preloading_data.h"
#include "content/public/common/content_features.h"
#include "net/http/http_no_vary_search_data.h"
#include "third_party/blink/public/mojom/loader/referrer.mojom.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/flags/android/chrome_feature_list.h"
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
constexpr size_t kMaxNumberOfCCTPrefetches = 50;
#endif  // BUILDFLAG(IS_ANDROID)

ChromePrefetchManager::~ChromePrefetchManager() = default;

#if BUILDFLAG(IS_ANDROID)
void ChromePrefetchManager::StartPrefetchFromCCT(
    const GURL& prefetch_url,
    bool use_prefetch_proxy,
    const std::optional<url::Origin>& referring_origin) {
  if (!base::FeatureList::IsEnabled(
          chrome::android::kCCTNavigationalPrefetch)) {
    return;
  }
  auto* preloading_data =
      content::PreloadingData::GetOrCreateForWebContents(&GetWebContents());

  // Normally, prefetch uses `PrefetchServiceMatcher` for the NVS-aware matching
  // for `is_accurate_triggering_` performed on
  // `PreloadingDataImpl::DidStartNavigation`, but since CCT prefetch doesn't
  // support NVS, `SameURLMatcher` is sufficient here.
  content::PreloadingURLMatchCallback matcher =
      content::PreloadingData::GetSameURLMatcher(prefetch_url);

  // Regarding `triggering_primary_page_source_id`: Since the CCT prefetch's
  // trigger is Android App, it should be `ukm::kInvalidSourceId` (And if so,
  // `Preloading.Attempt.PreviousPrimaryPage` will not be recorded).
  content::PreloadingAttempt* preloading_attempt =
      preloading_data->AddPreloadingAttempt(
          chrome_preloading_predictor::kChromeCustomTabs,
          content::PreloadingType::kPrefetch, std::move(matcher),
          /*triggering_primary_page_source_id=*/ukm::kInvalidSourceId);

  content::PreloadingHoldbackStatus holdback_status_override =
      content::PreloadingHoldbackStatus::kUnspecified;
  if (chrome::android::kCCTNavigationalPrefetchHoldback.Get()) {
    holdback_status_override = content::PreloadingHoldbackStatus::kHoldback;
  }

  // TODO(crbug.com/40288091): Specify appropriate referrer value that comes
  // from CCT.
  std::unique_ptr<content::PrefetchHandle> prefetch_handle =
      GetWebContents().StartPrefetch(
          prefetch_url, use_prefetch_proxy, kCCTMetricsSuffix,
          blink::mojom::Referrer(), referring_origin,
          /*no_vary_search_hint=*/std::nullopt,
          /*priority=*/std::nullopt,
          content::PreloadPipelineInfo::Create(
              /*planned_max_preloading_type=*/content::PreloadingType::
                  kPrefetch),
          preloading_attempt->GetWeakPtr(), holdback_status_override,
          /*ttl=*/std::nullopt);
  if (prefetch_handle) {
    if (all_prefetches_.size() >= kMaxNumberOfCCTPrefetches) {
      all_prefetches_.pop_front();
    }
    all_prefetches_.push_back(std::move(prefetch_handle));
  }
}
#endif  // BUILDFLAG(IS_ANDROID)

ChromePrefetchManager::ChromePrefetchManager(content::WebContents* web_contents)
    : content::WebContentsUserData<ChromePrefetchManager>(*web_contents) {}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ChromePrefetchManager);
