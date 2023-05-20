// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "android_webview/browser_jni_headers/AwFeatureMap_jni.h"
#include "android_webview/common/aw_features.h"
#include "base/android/feature_map.h"
#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "components/embedder_support/android/metrics/features.h"
#include "components/safe_browsing/core/common/features.h"

namespace android_webview {

namespace {
// Array of features exposed through the Java AwFeatureList API. Entries in this
// array may either refer to features defined in
// android_webview/common/aw_features.cc or in other locations in the code base
// (e.g. content/, components/, etc).
const base::Feature* const kFeaturesExposedToJava[] = {
    &features::kWebViewConnectionlessSafeBrowsing,
    &features::kWebViewServerSideSampling,
    &features::kWebViewDisplayCutout,
    &features::kWebViewMixedContentAutoupgrades,
    &features::kWebViewTestFeature,
    &features::kWebViewJavaJsBridgeMojo,
    &features::kWebViewUseMetricsUploadService,
    &features::kWebViewXRequestedWithHeaderControl,
    &features::kWebViewXRequestedWithHeaderManifestAllowList,
    &features::kWebViewRestrictSensitiveContent,
    &features::kWebViewUmaUploadQualityOfServiceSetToDefault,
    &metrics::kAndroidMetricsAsyncMetricLogging,
    &features::kWebViewZoomKeyboardShortcuts,
};

// static
base::android::FeatureMap* GetFeatureMap() {
  static base::NoDestructor<base::android::FeatureMap> kFeatureMap(std::vector(
      std::begin(kFeaturesExposedToJava), std::end(kFeaturesExposedToJava)));
  return kFeatureMap.get();
}

}  // namespace

static jlong JNI_AwFeatureMap_GetNativeMap(JNIEnv* env) {
  return reinterpret_cast<jlong>(GetFeatureMap());
}

}  // namespace android_webview
