// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/customtabs/custom_tab_session_state_tracker.h"

#include "base/android/jni_string.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/CustomTabsOpenTimeRecorder_jni.h"

namespace chrome {
namespace android {

// static
CustomTabSessionStateTracker& CustomTabSessionStateTracker::GetInstance() {
  static base::NoDestructor<CustomTabSessionStateTracker> instance;
  return *instance;
}

bool CustomTabSessionStateTracker::HasCustomTabSessionState() const {
  return has_custom_tab_session_;
}

void CustomTabSessionStateTracker::RecordCustomTabSession(
    int64_t time_sec,
    std::string package_name,
    int32_t session_duration,
    bool was_user_closed,
    bool is_partial) {
  has_custom_tab_session_ = true;
  custom_tab_session_ = std::make_unique<metrics::CustomTabSessionProto>();
  custom_tab_session_->set_time_sec(time_sec);
  custom_tab_session_->set_package_name(package_name);
  custom_tab_session_->set_session_duration_sec(session_duration);
  custom_tab_session_->set_did_user_interact(did_user_interact_);
  custom_tab_session_->set_was_user_closed(was_user_closed);
  custom_tab_session_->set_is_partial(is_partial);
}

std::unique_ptr<metrics::CustomTabSessionProto>
CustomTabSessionStateTracker::GetSession() {
  // Clear the session because we only want the state recorded once.
  has_custom_tab_session_ = false;

  return std::move(custom_tab_session_);
}

void CustomTabSessionStateTracker::OnUserInteraction() {
  did_user_interact_ = true;
}

CustomTabSessionStateTracker::CustomTabSessionStateTracker() = default;
CustomTabSessionStateTracker::~CustomTabSessionStateTracker() = default;

}  // namespace android
}  // namespace chrome

static void JNI_CustomTabsOpenTimeRecorder_RecordCustomTabSession(
    JNIEnv* env,
    jlong j_time,
    const base::android::JavaParamRef<jstring>& j_package_name,
    jlong j_session_duration,
    jboolean j_was_user_closed,
    jboolean j_is_partial_cct) {
  std::string package_name =
      base::android::ConvertJavaStringToUTF8(env, j_package_name);
  chrome::android::CustomTabSessionStateTracker::GetInstance()
      .RecordCustomTabSession(j_time, package_name, j_session_duration,
                              j_was_user_closed, j_is_partial_cct);
}
