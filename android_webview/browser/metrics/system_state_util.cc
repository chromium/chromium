// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/metrics/system_state_util.h"

#include <string>

#include "base/android/apk_info.h"
#include "base/android/jni_string.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "android_webview/browser_jni_headers/SystemStateUtil_jni.h"

namespace android_webview {

MultipleUserProfilesState GetMultipleUserProfilesState() {
  static MultipleUserProfilesState multiple_user_profiles_state =
      static_cast<MultipleUserProfilesState>(
          Java_SystemStateUtil_getMultipleUserProfilesState(
              jni_zero::AttachCurrentThread()));
  return multiple_user_profiles_state;
}

PrimaryCpuAbiBitness GetPrimaryCpuAbiBitness() {
  static PrimaryCpuAbiBitness primary_cpu_abi_bitness =
      static_cast<PrimaryCpuAbiBitness>(
          Java_SystemStateUtil_getPrimaryCpuAbiBitness(
              jni_zero::AttachCurrentThread()));
  return primary_cpu_abi_bitness;
}

std::optional<AgsaProcessName> GetAgsaProcessNameEnum() {
  if (base::android::apk_info::host_package_name() !=
      "com.google.android.googlequicksearchbox") {
    return std::nullopt;
  }
  std::string process_name = base::android::ConvertJavaStringToUTF8(
      Java_SystemStateUtil_getProcessName(jni_zero::AttachCurrentThread()));
  return internal::GetAgsaProcessNameEnumImpl(process_name);
}

namespace internal {
// Maps AGSA process name strings (e.g.
// "com.google.android.googlequicksearchbox:search") to their corresponding enum
// values for UMA logging.
AgsaProcessName GetAgsaProcessNameEnumImpl(std::string_view process_name) {
  if (process_name == "com.google.android.googlequicksearchbox:googleapp") {
    return AgsaProcessName::kGoogleApp;
  } else if (process_name == "com.google.android.googlequicksearchbox:search") {
    return AgsaProcessName::kSearch;
  } else if (process_name ==
             "com.google.android.googlequicksearchbox:interactor") {
    return AgsaProcessName::kInteractor;
  }
  return AgsaProcessName::kOther;
}
}  // namespace internal

}  // namespace android_webview

DEFINE_JNI(SystemStateUtil)
