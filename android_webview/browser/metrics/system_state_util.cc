// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/metrics/system_state_util.h"

#include <sys/system_properties.h>

#include <algorithm>
#include <array>
#include <string>
#include <string_view>
#include <vector>

#include "base/android/apk_info.h"
#include "base/android/jni_string.h"
#include "base/containers/span.h"
#include "base/strings/string_split.h"
#include "base/strings/string_view_util.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "android_webview/browser_jni_headers/SystemStateUtil_jni.h"

namespace android_webview {

namespace {
bool AbiPropListContains(const char* prop_name, std::string_view target_abi) {
  std::array<char, PROP_VALUE_MAX> abilist;
  int len = __system_property_get(prop_name, abilist.data());
  if (len <= 0) {
    return false;
  }
  std::string_view abilist_view =
      base::as_string_view(base::span(abilist).first(static_cast<size_t>(len)));
  std::vector<std::string_view> supported_abis = base::SplitStringPiece(
      abilist_view, ",", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  return std::ranges::contains(supported_abis, target_abi);
}

PrimaryCpuAbiBitness ComputePrimaryCpuAbiBitness() {
  const std::string& primary_abi = base::android::apk_info::primary_cpu_abi();
  if (primary_abi.empty()) {
    return PrimaryCpuAbiBitness::kUnknown;
  }
  if (AbiPropListContains("ro.product.cpu.abilist32", primary_abi)) {
    return PrimaryCpuAbiBitness::k32bit;
  }
  if (AbiPropListContains("ro.product.cpu.abilist64", primary_abi)) {
    return PrimaryCpuAbiBitness::k64bit;
  }
  return PrimaryCpuAbiBitness::kUnknown;
}
}  // namespace

MultipleUserProfilesState GetMultipleUserProfilesState() {
  static MultipleUserProfilesState multiple_user_profiles_state =
      static_cast<MultipleUserProfilesState>(
          Java_SystemStateUtil_getMultipleUserProfilesState(
              jni_zero::AttachCurrentThread()));
  return multiple_user_profiles_state;
}

PrimaryCpuAbiBitness GetPrimaryCpuAbiBitness() {
  static const PrimaryCpuAbiBitness primary_cpu_abi_bitness =
      ComputePrimaryCpuAbiBitness();
  return primary_cpu_abi_bitness;
}

std::optional<AgsaProcessName> GetAgsaProcessNameEnum() {
  if (base::android::apk_info::host_package_name() !=
      "com.google.android.googlequicksearchbox") {
    return std::nullopt;
  }
  std::string process_name =
      Java_SystemStateUtil_getProcessName(jni_zero::AttachCurrentThread());
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
