// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/subresource_redirect/subresource_redirect_util.h"

#include "build/build_config.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings_factory.h"
#include "chrome/browser/subresource_redirect/https_image_compression_bypass_decider.h"
#include "chrome/browser/subresource_redirect/https_image_compression_infobar_decider.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/features.h"

#if defined(OS_ANDROID)
#include "chrome/browser/previews/android/previews_android_bridge.h"
#endif

namespace subresource_redirect {

namespace {

bool IsSubresourceRedirectEnabled() {
  return base::FeatureList::IsEnabled(blink::features::kSubresourceRedirect);
}

DataReductionProxyChromeSettings* GetDataReductionProxyChromeSettings(
    content::WebContents* web_contents) {
  DCHECK(base::FeatureList::IsEnabled(blink::features::kSubresourceRedirect));
  if (!web_contents)
    return nullptr;
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

}  // namespace

bool IsLiteModeEnabled(content::WebContents* web_contents) {
  if (!web_contents)
    return false;
  const auto* data_reduction_proxy_settings =
      GetDataReductionProxyChromeSettings(web_contents);
  return data_reduction_proxy_settings &&
         data_reduction_proxy_settings->IsDataReductionProxyEnabled();
}

bool ShouldEnablePublicImageHintsBasedCompression() {
  bool is_enabled = IsSubresourceRedirectEnabled() &&
                    base::GetFieldTrialParamByFeatureAsBool(
                        blink::features::kSubresourceRedirect,
                        "enable_public_image_hints_based_compression", true);
  // Only one of the public image hints or login and robots based image
  // compression should be active.
  DCHECK(!is_enabled || !ShouldEnableLoginRobotsCheckedCompression());
  return is_enabled;
}

bool ShouldEnableLoginRobotsCheckedCompression() {
  bool is_enabled = IsSubresourceRedirectEnabled() &&
                    base::GetFieldTrialParamByFeatureAsBool(
                        blink::features::kSubresourceRedirect,
                        "enable_login_robots_based_compression", false);
  // Only one of the public image hints or login and robots based image
  // compression should be active.
  DCHECK(!is_enabled || !ShouldEnablePublicImageHintsBasedCompression());
  return is_enabled;
}

// Should the subresource be redirected to its compressed version. This returns
// false if only coverage metrics need to be recorded and actual redirection
// should not happen.
bool ShouldCompressRedirectSubresource() {
  return base::FeatureList::IsEnabled(blink::features::kSubresourceRedirect) &&
         base::GetFieldTrialParamByFeatureAsBool(
             blink::features::kSubresourceRedirect,
             "enable_subresource_server_redirect", true);
}

bool ShowInfoBarAndGetImageCompressionState(
    content::WebContents* web_contents,
    content::NavigationHandle* navigation_handle) {
  auto* data_reduction_proxy_settings =
      GetDataReductionProxyChromeSettings(web_contents);
  if (!data_reduction_proxy_settings->IsDataReductionProxyEnabled()) {
    return false;
  }

  if (data_reduction_proxy_settings->https_image_compression_bypass_decider()
          ->ShouldBypassNow()) {
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
      ->https_image_compression_bypass_decider()
      ->NotifyCompressedImageFetchFailed(retry_after);
}

}  // namespace subresource_redirect
