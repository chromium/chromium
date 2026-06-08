// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/common/aw_feature_map.h"

#include <string>

#include "android_webview/common/aw_features.h"
#include "base/android/feature_map.h"
#include "base/feature_list.h"
#include "base/features.h"
#include "base/no_destructor.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/sensitive_content/features.h"
#include "components/viz/common/features.h"
#include "content/public/common/content_features.h"
#include "services/tracing/public/cpp/tracing_features.h"
#include "third_party/blink/public/common/features.h"

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
    &::features::kEnablePerfettoSystemTracing,
    &safe_browsing::kHashPrefixRealTimeLookups,
    &features::kPostChromiumStartupInWebViewConstructor,
    &base::features::kPostGetMyMemoryStateToBackground,
    &sensitive_content::features::kSensitiveContent,
    &base::features::kShutdownPreNativeThreadPoolAfterStartup,
    &features::kStartupNonBlockingWebViewConstructor,
    &features::kWebViewAddQuicHints,
    &features::kWebViewBackForwardCache,
    &features::kWebViewBackgroundClassPreloading,
    &features::kWebViewBackgroundTracingInit,
    &features::kWebViewBypassProvisionalCookieManager,
    &features::kWebViewCacheSizeLimitDerivedFromAppCacheQuota,
    &features::kWebViewContentRestrictionSupport,
    &features::kWebViewCppMetricsFiltering,
    &features::kWebViewEarlyStartupTracing,
    &features::kWebViewEarlyTracingInit,
    &features::kWebViewEnableApiCallUserActions,
    &features::kWebViewEnableCrash,
    &features::kWebViewEnableDnsPlatform,
    &features::kWebViewFasterGetDefaultUserAgent,
    &features::kWebViewFileSystemAccess,
    &features::kWebViewForceWebAuthn,
    &features::kWebViewHyperlinkContextMenu,
    &features::kWebViewInvokeZoomPickerOnGSU,
    &features::kWebViewMixedContentAutoupgrades,
    &features::kWebViewMoveWorkToProviderInit,
    &features::kWebViewNavigate,
    &features::kWebViewOptInToGmsBindServiceOptimization,
    &features::kWebViewPersistHttpServerProperties,
    &features::kWebViewPrefetchNativeLibrary,
    &features::kWebViewPrefetchOffTheMainThread,
    &features::kWebViewProfileStoreNotTriggerStartup,
    &features::kWebViewRecordAppCacheHistograms,
    &features::kWebViewReduceUAAndroidVersionDeviceModel,
    &features::kWebViewReducedSeedExpiration,
    &features::kWebViewReducedSeedRequestPeriod,
    &features::kWebViewRemoveInstantAppSupport,
    &features::kWebViewRendererKeepAlive,
    &features::kWebViewSkipInterceptsForPrefetch,
    &features::kWebViewStaticMethodsNotTriggerStartup,
    &features::kWebViewTestFeature,
    &features::kWebViewUseMetricsUploadServiceOnlySdkRuntime,
    &features::kWebViewUseNonembeddedLowEntropySource,
    // keep-sorted end
};

}  // namespace

// static
base::android::FeatureMap* GetFeatureMap() {
  static base::NoDestructor<base::android::FeatureMap> kFeatureMap(
      kFeaturesExposedToJava);
  return kFeatureMap.get();
}

static int64_t JNI_AwFeatureMap_GetNativeMap(JNIEnv* env) {
  return reinterpret_cast<int64_t>(GetFeatureMap());
}

}  // namespace android_webview

DEFINE_JNI(AwFeatureMap)
