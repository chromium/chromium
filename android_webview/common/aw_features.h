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
BASE_DECLARE_FEATURE(kWebViewAppsPackageNamesServerSideAllowlist);
BASE_DECLARE_FEATURE(kWebViewBrotliSupport);
BASE_DECLARE_FEATURE(kWebViewCheckReturnResources);
BASE_DECLARE_FEATURE(kWebViewServerSideSampling);
BASE_DECLARE_FEATURE(kWebViewConnectionlessSafeBrowsing);
BASE_DECLARE_FEATURE(kWebViewClearFunctorInBackground);
BASE_DECLARE_FEATURE(kWebViewDisplayCutout);
BASE_DECLARE_FEATURE(kWebViewEmptyComponentLoaderPolicy);
BASE_DECLARE_FEATURE(kWebViewEnumerateDevicesCache);
BASE_DECLARE_FEATURE(kWebViewExtraHeadersSameOriginOnly);
BASE_DECLARE_FEATURE(kWebViewForceDarkModeMatchTheme);
BASE_DECLARE_FEATURE(kWebViewHitTestInBlinkOnTouchStart);
BASE_DECLARE_FEATURE(kWebViewImageDrag);
BASE_DECLARE_FEATURE(kWebViewJavaJsBridgeMojo);
BASE_DECLARE_FEATURE(kWebViewMixedContentAutoupgrades);
BASE_DECLARE_FEATURE(kWebViewOriginTrials);
BASE_DECLARE_FEATURE(kWebViewRecordAppDataDirectorySize);
BASE_DECLARE_FEATURE(kWebViewReportFrameMetrics);
BASE_DECLARE_FEATURE(kWebViewRestrictSensitiveContent);
BASE_DECLARE_FEATURE(kWebViewSafeBrowsingSafeMode);
BASE_DECLARE_FEATURE(kWebViewSuppressDifferentOriginSubframeJSDialogs);
BASE_DECLARE_FEATURE(kWebViewTestFeature);
BASE_DECLARE_FEATURE(kWebViewUseMetricsUploadService);
BASE_DECLARE_FEATURE(kWebViewWideColorGamutSupport);
BASE_DECLARE_FEATURE(kWebViewXRequestedWithHeaderControl);
extern const base::FeatureParam<int> kWebViewXRequestedWithHeaderMode;
BASE_DECLARE_FEATURE(kWebViewXRequestedWithHeaderManifestAllowList);
BASE_DECLARE_FEATURE(kWebViewUmaUploadQualityOfServiceSetToDefault);
BASE_DECLARE_FEATURE(kWebViewZoomKeyboardShortcuts);

}  // namespace features
}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_COMMON_AW_FEATURES_H_
