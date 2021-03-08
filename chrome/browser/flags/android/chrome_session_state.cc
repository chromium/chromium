// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/flags/android/chrome_session_state.h"

#include "base/notreached.h"
#include "chrome/browser/flags/jni_headers/ChromeSessionState_jni.h"
#include "services/metrics/public/cpp/ukm_source.h"

using chrome::android::ActivityType;

namespace {
ActivityType activity_type = ActivityType::kTabbed;
bool is_in_multi_window_mode = false;
}  // namespace

namespace chrome {
namespace android {

CustomTabsVisibilityHistogram GetCustomTabsVisibleValue(
    ActivityType activity_type) {
  switch (activity_type) {
    case ActivityType::kTabbed:
    case ActivityType::kWebapp:
    case ActivityType::kWebApk:
      return VISIBLE_CHROME_TAB;
    case ActivityType::kCustomTab:
    case ActivityType::kTrustedWebActivity:
      return VISIBLE_CUSTOM_TAB;
  }
  NOTREACHED();
  return VISIBLE_CHROME_TAB;
}

ActivityType GetActivityType() {
  return activity_type;
}

bool GetIsInMultiWindowModeValue() {
  return is_in_multi_window_mode;
}

}  // namespace android
}  // namespace chrome

static void JNI_ChromeSessionState_SetActivityType(JNIEnv* env, jint type) {
  activity_type = static_cast<ActivityType>(type);
  // TODO(peconn): Look into adding this for UKM as well.
  ukm::UkmSource::SetCustomTabVisible(
      GetCustomTabsVisibleValue(activity_type) ==
      chrome::android::VISIBLE_CUSTOM_TAB);
}

static void JNI_ChromeSessionState_SetIsInMultiWindowMode(
    JNIEnv* env,
    jboolean j_is_in_multi_window_mode) {
  is_in_multi_window_mode = j_is_in_multi_window_mode;
}
