// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/prefetch/chrome_prefetch_manager.h"

#include "content/public/common/content_features.h"
#include "third_party/blink/public/mojom/loader/referrer.mojom.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/flags/android/chrome_feature_list.h"
#endif  // BUILDFLAG(IS_ANDROID)

ChromePrefetchManager::~ChromePrefetchManager() = default;

// static
ChromePrefetchManager* ChromePrefetchManager::GetOrCreateForWebContents(
    content::WebContents* web_contents) {
  auto* chrome_prefetch_manager =
      ChromePrefetchManager::FromWebContents(web_contents);
  if (!chrome_prefetch_manager) {
    ChromePrefetchManager::CreateForWebContents(web_contents);
    chrome_prefetch_manager =
        ChromePrefetchManager::FromWebContents(web_contents);
  }

  return chrome_prefetch_manager;
}

#if BUILDFLAG(IS_ANDROID)
void ChromePrefetchManager::StartPrefetchFromCCT(
    const GURL& prefetch_url,
    bool use_prefetch_proxy,
    const std::optional<url::Origin>& referring_origin) {
  CHECK(
      base::FeatureList::IsEnabled(chrome::android::kCCTNavigationalPrefetch));

  // TODO(crbug.com/40288091): Prepare PreloadingPredictor / PreloadingAttempt
  // for CCT prefetches.
  // TODO(crbug.com/40288091): Specify appropriate referrer value that comes
  // from CCT.
  GetWebContents().StartPrefetch(prefetch_url, use_prefetch_proxy,
                                 blink::mojom::Referrer(), referring_origin,
                                 /*attempt=*/nullptr);
}
#endif  // BUILDFLAG(IS_ANDROID)

ChromePrefetchManager::ChromePrefetchManager(content::WebContents* web_contents)
    : content::WebContentsUserData<ChromePrefetchManager>(*web_contents) {
  CHECK(base::FeatureList::IsEnabled(
      features::kPrefetchBrowserInitiatedTriggers));
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ChromePrefetchManager);
