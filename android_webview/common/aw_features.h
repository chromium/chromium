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
BASE_DECLARE_FEATURE(kWebViewAddQuicHints);
BASE_DECLARE_FEATURE(kWebViewBackForwardCache);
// TODO(crbug.com/455296998): Remove this code for M145.
BASE_DECLARE_FEATURE(kWebViewBypassHttpCacheForPrefetchFromHeader);
BASE_DECLARE_FEATURE(kWebViewConfigurableLibraryPrefetch);
BASE_DECLARE_FEATURE(kWebViewFileSystemAccess);
BASE_DECLARE_FEATURE(kWebViewIgnoreDuplicateNavs);
BASE_DECLARE_FEATURE(kWebViewInvokeZoomPickerOnGSU);
BASE_DECLARE_FEATURE(kWebViewLazyFetchHandWritingIcon);
BASE_DECLARE_FEATURE(kWebViewMixedContentAutoupgrades);
BASE_DECLARE_FEATURE(kWebViewRenderDocument);
BASE_DECLARE_FEATURE(kWebViewTestFeature);
BASE_DECLARE_FEATURE(kWebViewUseMetricsUploadServiceOnlySdkRuntime);
BASE_DECLARE_FEATURE(kWebViewPropagateNetworkChangeSignals);
BASE_DECLARE_FEATURE(kWebViewStartupTasksYieldToNative);
BASE_DECLARE_FEATURE(kWebViewUnreducedProductVersion);
BASE_DECLARE_FEATURE(kWebViewUseRenderingHeuristic);
BASE_DECLARE_FEATURE(kWebViewUseStartupTasksLogic);
BASE_DECLARE_FEATURE(kWebViewUseStartupTasksLogicP2);
BASE_DECLARE_FEATURE(kWebViewUseInitialNetworkStateAtStartup);
BASE_DECLARE_FEATURE(kWebViewReduceUAAndroidVersionDeviceModel);
BASE_DECLARE_FEATURE(kWebViewEnableCrash);
BASE_DECLARE_FEATURE(kWebViewPrefetchNativeLibrary);
extern const base::FeatureParam<bool> kWebViewPrefetchFromRenderer;
BASE_DECLARE_FEATURE(kWebViewSkipInterceptsForPrefetch);
BASE_DECLARE_FEATURE(kWebViewHyperlinkContextMenu);
BASE_DECLARE_FEATURE(kCreateSpareRendererOnBrowserContextCreation);
BASE_DECLARE_FEATURE(kWebViewWebauthn);
BASE_DECLARE_FEATURE(kWebViewInterceptedCookieHeader);
BASE_DECLARE_FEATURE(kWebViewInterceptedCookieHeaderReadWrite);
BASE_DECLARE_FEATURE(kWebViewRecordAppCacheHistograms);
BASE_DECLARE_FEATURE(kWebViewCacheSizeLimitDerivedFromAppCacheQuota);
extern const base::FeatureParam<double> kWebViewCacheSizeLimitMultiplier;
extern const base::FeatureParam<int> kWebViewCacheSizeLimitMinimum;
extern const base::FeatureParam<int> kWebViewCacheSizeLimitMaximum;
extern const base::FeatureParam<double> kWebViewCodeCacheSizeLimitMultiplier;
BASE_DECLARE_FEATURE(kWebViewConnectToComponentProviderInBackground);
BASE_DECLARE_FEATURE(kAndroidMetricsAsyncMetricLogging);
BASE_DECLARE_FEATURE(kWebViewReducedSeedExpiration);
BASE_DECLARE_FEATURE(kWebViewReducedSeedRequestPeriod);
BASE_DECLARE_FEATURE(kWebViewEarlyStartupTracing);
BASE_DECLARE_FEATURE(kWebViewEarlyPerfettoInit);
BASE_DECLARE_FEATURE(kWebViewCacheBoundaryInterfaceMethods);
BASE_DECLARE_FEATURE(kWebViewOptInToGmsBindServiceOptimization);
BASE_DECLARE_FEATURE(kWebViewMoveWorkToProviderInit);
BASE_DECLARE_FEATURE(kWebViewBypassProvisionalCookieManager);
BASE_DECLARE_FEATURE(kWebViewPersistentMetricsInNoBackupDir);
BASE_DECLARE_FEATURE(kPrerender2WarmUpCompositorForWebView);
BASE_DECLARE_FEATURE(kWebViewFetchOriginTrialsComponent);

}  // namespace android_webview::features

#endif  // ANDROID_WEBVIEW_COMMON_AW_FEATURES_H_
