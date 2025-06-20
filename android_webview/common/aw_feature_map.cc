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
#include "android_webview/common_jni/AwFeatureMap_jni.h"

namespace android_webview {

namespace {
// Array of features exposed through the Java AwFeatureList API. Entries in this
// array may either refer to features defined in
// android_webview/common/aw_features.cc or in other locations in the code base
// (e.g. content/, components/, etc).
const base::Feature* const kFeaturesExposedToJava[] = {
    // Ordered alphabetically on feature name.
    // keep-sorted start allow_yaml_lists=yes by_regex=['\w+,']
    &metrics::kAndroidMetricsAsyncMetricLogging,
    &base::features::kCollectAndroidFrameTimelineMetrics,
    &safe_browsing::kHashPrefixRealTimeLookups,
    &base::features::kPostGetMyMemoryStateToBackground,
    &::features::kPrefetchBrowserInitiatedTriggers,
    &sensitive_content::features::kSensitiveContent,
    &features::kWebViewBackForwardCache,
    &features::kWebViewCacheSizeLimitDerivedFromAppCacheQuota,
    &features::kWebViewConnectToComponentProviderInBackground,
    &features::kWebViewDisableCHIPS,
    &features::kWebViewDoNotSendAccessibilityEventsOnGSU,
    &features::kWebViewDrainPrefetchQueueDuringInit,
    &features::kWebViewEnableCrash,
    &features::kWebViewFileSystemAccess,
    &features::kWebViewHyperlinkContextMenu,
    &features::kWebViewInvokeZoomPickerOnGSU,
    &features::kWebViewLazyFetchHandWritingIcon,
    &features::kWebViewMediaIntegrityApiBlinkExtension,
    &features::kWebViewMixedContentAutoupgrades,
    &features::kWebViewMuteAudio,
    &features::kWebViewPrefetchNativeLibrary,
    &features::kWebViewPreloadClasses,
    &features::kWebViewQuicConnectionTimeout,
    &features::kWebViewRecordAppCacheHistograms,
    &features::kWebViewReduceUAAndroidVersionDeviceModel,
    &features::kWebViewSafeAreaIncludesSystemBars,
    &features::kWebViewSeparateResourceContext,
    &features::kWebViewShortCircuitShouldInterceptRequest,
    &features::kWebViewSkipInterceptsForPrefetch,
    &features::kWebViewTestFeature,
    &features::kWebViewUseInitialNetworkStateAtStartup,
    &features::kWebViewUseMetricsUploadService,
    &features::kWebViewUseMetricsUploadServiceOnlySdkRuntime,
    &features::kWebViewUseStartupTasksLogic,
    &features::kWebViewWebauthn,
    &features::kWebViewXRequestedWithHeaderControl,
    // keep-sorted end
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
