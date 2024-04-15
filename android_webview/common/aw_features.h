// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_COMMON_AW_FEATURES_H_
#define ANDROID_WEBVIEW_COMMON_AW_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"

namespace android_webview {
namespace features {

// All features in alphabetical order. The features should be documented
// alongside the definition of their values in the .cc file.

// Alphabetical:
BASE_DECLARE_FEATURE(kWebViewBackForwardCache);
BASE_DECLARE_FEATURE(kWebViewBrotliSupport);
BASE_DECLARE_FEATURE(kWebViewCheckPakFileDescriptors);
BASE_DECLARE_FEATURE(kWebViewClearFunctorInBackground);
BASE_DECLARE_FEATURE(kWebViewDisplayCutout);
BASE_DECLARE_FEATURE(kWebViewEmptyComponentLoaderPolicy);
BASE_DECLARE_FEATURE(kWebViewEnumerateDevicesCache);
BASE_DECLARE_FEATURE(kWebViewExitReasonMetric);
BASE_DECLARE_FEATURE(kWebViewExtraHeadersSameOriginOnly);
BASE_DECLARE_FEATURE(kWebViewForceDarkModeMatchTheme);
BASE_DECLARE_FEATURE(kWebViewHitTestInBlinkOnTouchStart);
BASE_DECLARE_FEATURE(kWebViewImageDrag);
BASE_DECLARE_FEATURE(kWebViewInjectPlatformJsApis);
// Feature parameter for `network::features::kMaskedDomainList` which is
// defined in //services/network.
extern const base::FeatureParam<int> kWebViewIpProtectionExclusionCriteria;
BASE_DECLARE_FEATURE(kWebViewJavaJsBridgeMojo);
BASE_DECLARE_FEATURE(kWebViewMediaIntegrityApi);
BASE_DECLARE_FEATURE(kWebViewMediaIntegrityApiBlinkExtension);
BASE_DECLARE_FEATURE(kWebViewMixedContentAutoupgrades);
BASE_DECLARE_FEATURE(kWebViewMuteAudio);
BASE_DECLARE_FEATURE(kWebViewRecordAppDataDirectorySize);
BASE_DECLARE_FEATURE(kWebViewRestrictSensitiveContent);
BASE_DECLARE_FEATURE(kWebViewSupervisedUserSiteDetection);
BASE_DECLARE_FEATURE(kWebViewSupervisedUserSiteBlock);
BASE_DECLARE_FEATURE(kWebViewSuppressDifferentOriginSubframeJSDialogs);
BASE_DECLARE_FEATURE(kWebViewTestFeature);
BASE_DECLARE_FEATURE(kWebViewUseMetricsUploadService);
BASE_DECLARE_FEATURE(kWebViewUseMetricsUploadServiceOnlySdkRuntime);
BASE_DECLARE_FEATURE(kWebViewPrerender2);
BASE_DECLARE_FEATURE(kWebViewPropagateNetworkChangeSignals);
BASE_DECLARE_FEATURE(kWebViewUnreducedProductVersion);
BASE_DECLARE_FEATURE(kWebViewWideColorGamutSupport);
BASE_DECLARE_FEATURE(kWebViewXRequestedWithHeaderControl);
extern const base::FeatureParam<int> kWebViewXRequestedWithHeaderMode;
BASE_DECLARE_FEATURE(kWebViewXRequestedWithHeaderManifestAllowList);
BASE_DECLARE_FEATURE(kWebViewUmaUploadQualityOfServiceSetToDefault);
BASE_DECLARE_FEATURE(kWebViewUseInitialNetworkStateAtStartup);
BASE_DECLARE_FEATURE(kWebViewZoomKeyboardShortcuts);
BASE_DECLARE_FEATURE(kWebViewReduceUAAndroidVersionDeviceModel);
BASE_DECLARE_FEATURE(kWebViewEnableCrash);
BASE_DECLARE_FEATURE(kWebViewAsyncDns);

}  // namespace features
}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_COMMON_AW_FEATURES_H_
