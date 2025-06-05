// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_COMMON_AW_FEATURES_H_
#define ANDROID_WEBVIEW_COMMON_AW_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"

namespace android_webview::features {

// All features in alphabetical order. The features should be documented
// alongside the definition of their values in the .cc file.

// Alphabetical:
BASE_DECLARE_FEATURE(kWebViewAutoSAA);
BASE_DECLARE_FEATURE(kWebViewBackForwardCache);
BASE_DECLARE_FEATURE(kWebViewDigitalAssetLinksLoadIncludes);
BASE_DECLARE_FEATURE(kWebViewDisableCHIPS);
BASE_DECLARE_FEATURE(kWebViewDisableSharpeningAndMSAA);
BASE_DECLARE_FEATURE(kWebViewDrainPrefetchQueueDuringInit);
BASE_DECLARE_FEATURE(kWebViewFileSystemAccess);
BASE_DECLARE_FEATURE(kWebViewInvokeZoomPickerOnGSU);
// Feature parameter for `network::features::kMaskedDomainList` which is
// defined in //services/network.
extern const base::FeatureParam<int> kWebViewIpProtectionExclusionCriteria;
BASE_DECLARE_FEATURE(kWebViewLazyFetchHandWritingIcon);
BASE_DECLARE_FEATURE(kWebViewMediaIntegrityApiBlinkExtension);
BASE_DECLARE_FEATURE(kWebViewMixedContentAutoupgrades);
BASE_DECLARE_FEATURE(kWebViewMuteAudio);
BASE_DECLARE_FEATURE(kWebViewRecordAppDataDirectorySize);
BASE_DECLARE_FEATURE(kWebViewRenderDocument);
BASE_DECLARE_FEATURE(kWebViewTestFeature);
BASE_DECLARE_FEATURE(kWebViewUseMetricsUploadService);
BASE_DECLARE_FEATURE(kWebViewUseMetricsUploadServiceOnlySdkRuntime);
BASE_DECLARE_FEATURE(kWebViewPropagateNetworkChangeSignals);
BASE_DECLARE_FEATURE(kWebViewUnreducedProductVersion);
BASE_DECLARE_FEATURE(kWebViewUseStartupTasksLogic);
BASE_DECLARE_FEATURE(kWebViewXRequestedWithHeaderControl);
extern const base::FeatureParam<int> kWebViewXRequestedWithHeaderMode;
BASE_DECLARE_FEATURE(kWebViewUseInitialNetworkStateAtStartup);
BASE_DECLARE_FEATURE(kWebViewReduceUAAndroidVersionDeviceModel);
BASE_DECLARE_FEATURE(kWebViewEnableCrash);
BASE_DECLARE_FEATURE(kWebViewPreloadClasses);
BASE_DECLARE_FEATURE(kWebViewPrefetchNativeLibrary);
extern const base::FeatureParam<bool> kWebViewPrefetchFromRenderer;
BASE_DECLARE_FEATURE(kWebViewSafeAreaIncludesSystemBars);
BASE_DECLARE_FEATURE(kWebViewSeparateResourceContext);
BASE_DECLARE_FEATURE(kWebViewSkipInterceptsForPrefetch);
BASE_DECLARE_FEATURE(kWebViewDoNotSendAccessibilityEventsOnGSU);
BASE_DECLARE_FEATURE(kWebViewHyperlinkContextMenu);
BASE_DECLARE_FEATURE(kCreateSpareRendererOnBrowserContextCreation);
BASE_DECLARE_FEATURE(kWebViewWebauthn);
BASE_DECLARE_FEATURE(kWebViewInterceptedCookieHeader);
BASE_DECLARE_FEATURE(kWebViewInterceptedCookieHeaderReadWrite);
BASE_DECLARE_FEATURE(kWebViewShortCircuitShouldInterceptRequest);
BASE_DECLARE_FEATURE(kWebViewRecordAppCacheHistograms);
BASE_DECLARE_FEATURE(kWebViewQuicConnectionTimeout);
extern const base::FeatureParam<int> kWebViewQuicConnectionTimeoutSeconds;
BASE_DECLARE_FEATURE(kWebViewCacheSizeLimitDerivedFromAppCacheQuota);
extern const base::FeatureParam<double> kWebViewCacheSizeLimitMultiplier;
extern const base::FeatureParam<int> kWebViewCacheSizeLimitMinimum;
extern const base::FeatureParam<int> kWebViewCacheSizeLimitMaximum;
extern const base::FeatureParam<double> kWebViewCodeCacheSizeLimitMultiplier;
}  // namespace android_webview::features

#endif  // ANDROID_WEBVIEW_COMMON_AW_FEATURES_H_
