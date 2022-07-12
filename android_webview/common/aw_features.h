// Copyright 2018 The Chromium Authors. All rights reserved.
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
extern const base::Feature kWebViewBrotliSupport;
extern const base::Feature kWebViewConnectionlessSafeBrowsing;
extern const base::Feature kWebViewDisplayCutout;
extern const base::Feature kWebViewEmptyComponentLoaderPolicy;
extern const base::Feature kWebViewExtraHeadersSameOriginOnly;
extern const base::Feature kWebViewForceDarkModeMatchTheme;
extern const base::Feature kWebViewHitTestInBlinkOnTouchStart;
extern const base::Feature kWebViewJavaJsBridgeMojo;
extern const base::Feature kWebViewLegacyTlsSupport;
extern const base::Feature kWebViewMeasureScreenCoverage;
extern const base::Feature kWebViewMixedContentAutoupgrades;
extern const base::Feature kWebViewOriginTrials;
extern const base::Feature kWebViewRecordAppDataDirectorySize;
extern const base::Feature kWebViewSuppressDifferentOriginSubframeJSDialogs;
extern const base::Feature kWebViewTestFeature;
extern const base::Feature kWebViewUseMetricsUploadService;
extern const base::Feature kWebViewWideColorGamutSupport;
extern const base::Feature kWebViewXRequestedWithHeader;
extern const base::FeatureParam<int> kWebViewXRequestedWithHeaderMode;
extern const base::Feature
    kWebViewSynthesizePageLoadOnlyOnInitialMainDocumentAccess;

}  // namespace features
}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_COMMON_AW_FEATURES_H_
