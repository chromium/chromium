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
BASE_DECLARE_FEATURE(kWebViewBackgroundClassPreloading);
BASE_DECLARE_FEATURE(kWebViewBackgroundTracingInit);
// TODO(crbug.com/455296998): Remove this code for M145.
BASE_DECLARE_FEATURE(kWebViewBypassHttpCacheForPrefetchFromHeader);
BASE_DECLARE_FEATURE(kWebViewCppMetricsFiltering);
BASE_DECLARE_FEATURE(kWebViewContentRestrictionSupport);
BASE_DECLARE_FEATURE(kWebViewEarlyStartupTracing);
BASE_DECLARE_FEATURE(kWebViewEarlyTracingInit);
BASE_DECLARE_FEATURE(kWebViewEnableDnsPlatform);
BASE_DECLARE_FEATURE(kWebViewFileSystemAccess);
BASE_DECLARE_FEATURE(kWebViewForceWebAuthn);
BASE_DECLARE_FEATURE(kWebViewInvokeZoomPickerOnGSU);
BASE_DECLARE_FEATURE(kWebViewProfileStoreNotTriggerStartup);
BASE_DECLARE_FEATURE(kWebViewLatchedCookiePolicy);
BASE_DECLARE_FEATURE(kWebViewMixedContentAutoupgrades);
BASE_DECLARE_FEATURE(kWebViewNonBlockingCookieStoreHandoff);
BASE_DECLARE_FEATURE(kWebViewRenderDocument);
BASE_DECLARE_FEATURE(kWebViewTestFeature);
BASE_DECLARE_FEATURE(kWebViewUseMetricsUploadServiceOnlySdkRuntime);
BASE_DECLARE_FEATURE(kWebViewPropagateNetworkChangeSignals);
BASE_DECLARE_FEATURE(kWebViewUnreducedProductVersion);
BASE_DECLARE_FEATURE(kWebViewReduceUAAndroidVersionDeviceModel);
BASE_DECLARE_FEATURE(kWebViewEnableCrash);
BASE_DECLARE_FEATURE(kWebViewPrefetchAheadOfPrerender);
BASE_DECLARE_FEATURE(kWebViewPrefetchNativeLibrary);
extern const base::FeatureParam<bool> kWebViewPrefetchFromRenderer;
BASE_DECLARE_FEATURE(kWebViewPrefetchOnRendererReuse);
BASE_DECLARE_FEATURE(kWebViewPrefetchOffTheMainThread);
BASE_DECLARE_FEATURE(kWebViewPreloadServingMetrics);
BASE_DECLARE_FEATURE(kWebViewSkipInterceptsForPrefetch);
BASE_DECLARE_FEATURE(kWebViewHyperlinkContextMenu);
BASE_DECLARE_FEATURE(kWebViewVizDirectCompositorThreadIpcFrameSinkManager);
BASE_DECLARE_FEATURE(kWebViewInterceptedCookieHeader);
BASE_DECLARE_FEATURE(kWebViewInterceptedCookieHeaderReadWrite);
BASE_DECLARE_FEATURE(kWebViewRecordAppCacheHistograms);
BASE_DECLARE_FEATURE(kWebViewCacheSizeLimitDerivedFromAppCacheQuota);
extern const base::FeatureParam<double> kWebViewCacheSizeLimitMultiplier;
extern const base::FeatureParam<int> kWebViewCacheSizeLimitMinimum;
extern const base::FeatureParam<int> kWebViewCacheSizeLimitMaximum;
extern const base::FeatureParam<double> kWebViewCodeCacheSizeLimitMultiplier;
BASE_DECLARE_FEATURE(kWebViewReducedSeedExpiration);
BASE_DECLARE_FEATURE(kWebViewReducedSeedRequestPeriod);
BASE_DECLARE_FEATURE(kWebViewOptInToGmsBindServiceOptimization);
BASE_DECLARE_FEATURE(kWebViewMoveWorkToProviderInit);
BASE_DECLARE_FEATURE(kWebViewBypassProvisionalCookieManager);
BASE_DECLARE_FEATURE(kWebViewPersistentMetricsInNoBackupDir);
BASE_DECLARE_FEATURE(kPrerender2WarmUpCompositorForWebView);
BASE_DECLARE_FEATURE(kWebViewRendererKeepAlive);
extern const base::FeatureParam<base::TimeDelta>
    kWebViewRendererKeepAliveDuration;
BASE_DECLARE_FEATURE(kWebViewEnableApiCallUserActions);
BASE_DECLARE_FEATURE(kWebViewWebPerformanceMetricsReporting);
BASE_DECLARE_FEATURE(kWebViewTestNonembeddedLowEntropySource);
BASE_DECLARE_FEATURE(kWebViewUseNonembeddedLowEntropySource);
BASE_DECLARE_FEATURE(kWebViewFasterGetDefaultUserAgent);
BASE_DECLARE_FEATURE(kWebViewSaveStateIncludeHeaders);
BASE_DECLARE_FEATURE(kWebViewStaticMethodsNotTriggerStartup);
BASE_DECLARE_FEATURE(kStartupNonBlockingWebViewConstructor);
BASE_DECLARE_FEATURE(kPostChromiumStartupInWebViewConstructor);
BASE_DECLARE_FEATURE(kWebViewPersistHttpServerProperties);
BASE_DECLARE_FEATURE(kWebViewRemoveInstantAppSupport);
BASE_DECLARE_FEATURE(kWebViewNavigate);
BASE_DECLARE_FEATURE(kWebViewSetDownloadFaviconsEnabled);
BASE_DECLARE_FEATURE(kWebViewHttpCacheQuotaApi);
extern const base::FeatureParam<bool> kWebViewHttpCacheQuotaApiAllowShrinking;
extern const base::FeatureParam<bool>
    kWebViewHttpCacheQuotaApiAllowForDefaultProfile;
extern const base::FeatureParam<bool> kWebViewHttpCacheQuotaApiRuntimeUpdate;
extern const base::FeatureParam<int> kWebViewHttpCacheQuotaApiMinimum;
extern const base::FeatureParam<int> kWebViewHttpCacheQuotaApiMaximum;
extern const base::FeatureParam<bool> kWebViewHttpCacheQuotaApiAffectsCodeCache;
extern const base::FeatureParam<bool> kWebViewHttpCacheQuotaApiForceBackendInit;

}  // namespace android_webview::features

#endif  // ANDROID_WEBVIEW_COMMON_AW_FEATURES_H_
