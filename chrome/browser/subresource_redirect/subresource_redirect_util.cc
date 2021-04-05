// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/subresource_redirect/subresource_redirect_util.h"

#include "base/rand_util.h"
#include "build/build_config.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings_factory.h"
#include "chrome/browser/subresource_redirect/https_image_compression_infobar_decider.h"
#include "chrome/browser/subresource_redirect/litepages_service_bypass_decider.h"
#include "chrome/browser/subresource_redirect/origin_robots_rules_cache.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings.h"
#include "components/subresource_redirect/common/subresource_redirect_features.h"
#include "content/public/browser/web_contents.h"
#include "net/base/escape.h"
#include "third_party/blink/public/common/features.h"

#if defined(OS_ANDROID)
#include "chrome/browser/subresource_redirect/android/previews_android_bridge.h"
#endif

namespace subresource_redirect {

namespace {

DataReductionProxyChromeSettings* GetDataReductionProxyChromeSettings(
    content::WebContents* web_contents) {
  DCHECK(web_contents);
  return DataReductionProxyChromeSettingsFactory::GetForBrowserContext(
      web_contents->GetBrowserContext());
}

bool ShowInfoBarOnAndroid(content::WebContents* web_contents) {
#if defined(OS_ANDROID)
  return PreviewsAndroidBridge::CreateHttpsImageCompressionInfoBar(
      web_contents);
#endif
  return true;
}

// Returns the litepage robots origin from one of the image or src video
// subresource redirect features.
GURL GetLitePageRobotsOrigin() {
  auto lite_page_robots_origin = base::GetFieldTrialParamValueByFeature(
      blink::features::kSubresourceRedirect, "lite_page_robots_origin");
  if (lite_page_robots_origin.empty()) {
    lite_page_robots_origin = base::GetFieldTrialParamValueByFeature(
        blink::features::kSubresourceRedirectSrcVideo,
        "lite_page_robots_origin");
  }
  if (lite_page_robots_origin.empty())
    lite_page_robots_origin = "https://litepages.googlezip.net/";
  return GURL(lite_page_robots_origin);
}

}  // namespace

bool IsLiteModeEnabled(content::WebContents* web_contents) {
  if (!web_contents)
    return false;
  const auto* data_reduction_proxy_settings =
      GetDataReductionProxyChromeSettings(web_contents);
  return data_reduction_proxy_settings &&
         data_reduction_proxy_settings->IsDataReductionProxyEnabled();
}

bool ShowInfoBarAndGetImageCompressionState(
    content::WebContents* web_contents,
    content::NavigationHandle* navigation_handle) {
  DCHECK(ShouldEnablePublicImageHintsBasedCompression() ||
         ShouldEnableLoginRobotsCheckedImageCompression());

  auto* data_reduction_proxy_settings =
      GetDataReductionProxyChromeSettings(web_contents);
  if (!data_reduction_proxy_settings->IsDataReductionProxyEnabled()) {
    return false;
  }

  if (!data_reduction_proxy_settings->litepages_service_bypass_decider()
           ->ShouldAllowNow()) {
    return false;
  }

  auto* https_image_compression_infobar_decider =
      data_reduction_proxy_settings->https_image_compression_infobar_decider();
  if (!https_image_compression_infobar_decider ||
      https_image_compression_infobar_decider->NeedToShowInfoBar()) {
    if (https_image_compression_infobar_decider->CanShowInfoBar(
            navigation_handle) &&
        ShowInfoBarOnAndroid(web_contents)) {
      https_image_compression_infobar_decider->SetUserHasSeenInfoBar();
    }
    // Do not enable image compression on this page.
    return false;
  }
  return true;
}

void NotifyCompressedImageFetchFailed(content::WebContents* web_contents,
                                      base::TimeDelta retry_after) {
  GetDataReductionProxyChromeSettings(web_contents)
      ->litepages_service_bypass_decider()
      ->NotifyFetchFailure(retry_after);
}

GURL GetRobotsServerURL(const url::Origin& origin) {
  DCHECK(ShouldEnableRobotsRulesFetching());
  DCHECK(!origin.opaque());

  GURL origin_url = origin.GetURL();
  GURL::Replacements origin_replacement;
  origin_replacement.SetPathStr("/robots.txt");
  origin_url = origin_url.ReplaceComponents(origin_replacement);

  GURL lite_page_robots_url = GetLitePageRobotsOrigin();
  std::string query_str =
      "u=" + net::EscapeQueryParamValue(origin_url.spec(), true /* use_plus */);

  GURL::Replacements replacements;
  replacements.SetPathStr("/robots");
  replacements.SetQueryStr(query_str);

  lite_page_robots_url = lite_page_robots_url.ReplaceComponents(replacements);
  DCHECK(lite_page_robots_url.is_valid());
  return lite_page_robots_url;
}

OriginRobotsRulesCache* GetOriginRobotsRulesCache(
    content::WebContents* web_contents) {
  DCHECK(web_contents);
  if (const auto* data_reduction_proxy_settings =
          GetDataReductionProxyChromeSettings(web_contents)) {
    return data_reduction_proxy_settings->origin_robots_rules_cache();
  }
  return nullptr;
}

int MaxOriginRobotsRulesCacheSize() {
  return base::GetFieldTrialParamByFeatureAsInt(
      blink::features::kSubresourceRedirect,
      "max_browser_origin_robots_rules_cache_size", 20);
}

base::TimeDelta GetLitePagesBypassRandomDuration() {
  // Default is a random duration between 1 to 5 minutes.
  return base::TimeDelta::FromSeconds(
      base::RandInt(base::GetFieldTrialParamByFeatureAsInt(
                        blink::features::kSubresourceRedirect,
                        "litepages_bypass_random_duration_min_secs", 60),
                    base::GetFieldTrialParamByFeatureAsInt(
                        blink::features::kSubresourceRedirect,
                        "litepages_bypass_random_duration_max_secs", 300)));
}

base::TimeDelta GetLitePagesBypassMaxDuration() {
  return base::TimeDelta::FromSeconds(base::GetFieldTrialParamByFeatureAsInt(
      blink::features::kSubresourceRedirect,
      "litepages_bypass_max_duration_secs", 300));
}

}  // namespace subresource_redirect
