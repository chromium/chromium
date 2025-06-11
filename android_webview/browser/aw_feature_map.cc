// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "android_webview/common/aw_features.h"
#include "base/android/feature_map.h"
#include "base/feature_list.h"
#include "base/features.h"
#include "base/no_destructor.h"
#include "components/embedder_support/android/metrics/features.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/sensitive_content/features.h"
#include "components/viz/common/features.h"
#include "content/public/common/content_features.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "android_webview/browser_jni_headers/AwFeatureMap_jni.h"

namespace android_webview {

namespace {
// Array of features exposed through the Java AwFeatureList API. Entries in this
// array may either refer to features defined in
// android_webview/common/aw_features.cc or in other locations in the code base
// (e.g. content/, components/, etc).
const base::Feature* const kFeaturesExposedToJava[] = {
    &features::kWebViewBackForwardCache,
    &features::kWebViewDrainPrefetchQueueDuringInit,
    &features::kWebViewFileSystemAccess,
    &features::kWebViewInvokeZoomPickerOnGSU,
    &features::kWebViewLazyFetchHandWritingIcon,
    &features::kWebViewMixedContentAutoupgrades,
    &features::kWebViewTestFeature,
    &features::kWebViewUseMetricsUploadService,
    &features::kWebViewUseMetricsUploadServiceOnlySdkRuntime,
    &features::kWebViewXRequestedWithHeaderControl,
    &metrics::kAndroidMetricsAsyncMetricLogging,
    &safe_browsing::kHashPrefixRealTimeLookups,
    &base::features::kCollectAndroidFrameTimelineMetrics,
    &features::kWebViewMediaIntegrityApiBlinkExtension,
    &features::kWebViewSeparateResourceContext,
    &features::kWebViewSkipInterceptsForPrefetch,
    &features::kWebViewMuteAudio,
    &features::kWebViewUseInitialNetworkStateAtStartup,
    &features::kWebViewReduceUAAndroidVersionDeviceModel,
    &features::kWebViewEnableCrash,
    &features::kWebViewPreloadClasses,
    &features::kWebViewPrefetchNativeLibrary,
    &features::kWebViewDoNotSendAccessibilityEventsOnGSU,
    &features::kWebViewHyperlinkContextMenu,
    &features::kWebViewDisableCHIPS,
    &features::kWebViewSafeAreaIncludesSystemBars,
    &base::features::kPostGetMyMemoryStateToBackground,
    &sensitive_content::features::kSensitiveContent,
    &features::kWebViewWebauthn,
    &::features::kPrefetchBrowserInitiatedTriggers,
    &features::kWebViewShortCircuitShouldInterceptRequest,
    &features::kWebViewUseStartupTasksLogic,
    &features::kWebViewRecordAppCacheHistograms,
    &features::kWebViewQuicConnectionTimeout,
    &features::kWebViewCacheSizeLimitDerivedFromAppCacheQuota,
};

// static
base::android::FeatureMap* GetFeatureMap() {
  static base::NoDestructor<base::android::FeatureMap> kFeatureMap(
      kFeaturesExposedToJava);
  return kFeatureMap.get();
}

}  // namespace

static jlong JNI_AwFeatureMap_GetNativeMap(JNIEnv* env) {
  return reinterpret_cast<jlong>(GetFeatureMap());
}

}  // namespace android_webview
