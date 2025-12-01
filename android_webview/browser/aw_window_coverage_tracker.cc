// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_browser_process.h"
#include "android_webview/browser/metrics/visibility_metrics_logger.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/scoped_java_ref.h"

// Must come after JNI headers.
#include "android_webview/browser_jni_headers/AwWindowCoverageTracker_jni.h"

using base::android::AppendJavaStringArrayToStringVector;
using base::android::JavaIntArrayToIntVector;

namespace android_webview {

static void JNI_AwWindowCoverageTracker_UpdateScreenCoverage(
    JNIEnv* env,
    jint global_percentage,
    const base::android::JavaParamRef<jobjectArray>& jschemes,
    const base::android::JavaParamRef<jintArray>& jscheme_percentages) {
  std::vector<std::string> schemes;
  AppendJavaStringArrayToStringVector(env, jschemes, &schemes);

  std::vector<int> scheme_percentages;
  JavaIntArrayToIntVector(env, jscheme_percentages, &scheme_percentages);

  DCHECK(schemes.size() == scheme_percentages.size());

  std::vector<VisibilityMetricsLogger::Scheme> scheme_enums(schemes.size());
  for (size_t i = 0; i < schemes.size(); i++) {
    scheme_enums[i] = VisibilityMetricsLogger::SchemeStringToEnum(schemes[i]);
  }

  AwBrowserProcess::GetInstance()
      ->visibility_metrics_logger()
      ->UpdateScreenCoverage(global_percentage, scheme_enums,
                             scheme_percentages);
}

}  // namespace android_webview

DEFINE_JNI(AwWindowCoverageTracker)
