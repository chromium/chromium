// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/flags/android/chrome_session_state.h"

#include "chrome/browser/flags/jni_headers/ChromeSessionState_jni.h"

#include "services/metrics/public/cpp/ukm_source.h"

using chrome::android::ActivityType;

namespace {
bool custom_tab_visible = false;
ActivityType activity_type = ActivityType::kTabbed;
bool is_in_multi_window_mode = false;
}  // namespace

namespace chrome {
namespace android {

CustomTabsVisibilityHistogram GetCustomTabsVisibleValue() {
  return custom_tab_visible ? VISIBLE_CUSTOM_TAB : VISIBLE_CHROME_TAB;
}

ActivityType GetActivityType() {
  return activity_type;
}

bool GetIsInMultiWindowModeValue() {
  return is_in_multi_window_mode;
}

}  // namespace android
}  // namespace chrome

static void JNI_ChromeSessionState_SetCustomTabVisible(JNIEnv* env,
                                                       jboolean visible) {
  custom_tab_visible = visible;
  ukm::UkmSource::SetCustomTabVisible(visible);
}

static void JNI_ChromeSessionState_SetActivityType(JNIEnv* env, jint type) {
  activity_type = static_cast<ActivityType>(type);
  // TODO(peconn): Look into adding this for UKM as well.
}

static void JNI_ChromeSessionState_SetIsInMultiWindowMode(
    JNIEnv* env,
    jboolean j_is_in_multi_window_mode) {
  is_in_multi_window_mode = j_is_in_multi_window_mode;
}
