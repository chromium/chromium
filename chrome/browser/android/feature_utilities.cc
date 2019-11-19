// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/feature_utilities.h"

#include "chrome/android/chrome_jni_headers/FeatureUtilities_jni.h"

#include "base/android/jni_string.h"
#include "chrome/browser/ntp_snippets/content_suggestions_service_factory.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/ntp_snippets/content_suggestions_service.h"
#include "content/public/common/content_features.h"
#include "content/public/common/network_service_util.h"
#include "services/metrics/public/cpp/ukm_source.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace {
bool custom_tab_visible = false;
bool is_in_multi_window_mode = false;
} // namespace

namespace chrome {
namespace android {

CustomTabsVisibilityHistogram GetCustomTabsVisibleValue() {
  return custom_tab_visible ? VISIBLE_CUSTOM_TAB :
      VISIBLE_CHROME_TAB;
}

bool GetIsInMultiWindowModeValue() {
  return is_in_multi_window_mode;
}

bool IsDownloadAutoResumptionEnabledInNative() {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_FeatureUtilities_isDownloadAutoResumptionEnabledInNative(env);
}

std::string GetReachedCodeProfilerTrialGroup() {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> group =
      Java_FeatureUtilities_getReachedCodeProfilerTrialGroup(env);
  return ConvertJavaStringToUTF8(env, group);
}

} // namespace android
} // namespace chrome

static void JNI_FeatureUtilities_SetCustomTabVisible(
    JNIEnv* env,
    jboolean visible) {
  custom_tab_visible = visible;
  ukm::UkmSource::SetCustomTabVisible(visible);
}

static void JNI_FeatureUtilities_SetIsInMultiWindowMode(
    JNIEnv* env,
    jboolean j_is_in_multi_window_mode) {
  is_in_multi_window_mode = j_is_in_multi_window_mode;
}

static jboolean JNI_FeatureUtilities_IsNetworkServiceWarmUpEnabled(
    JNIEnv* env) {
  return content::IsOutOfProcessNetworkService() &&
         base::FeatureList::IsEnabled(features::kWarmUpNetworkProcess);
}
