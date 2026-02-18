// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_METRICS_SYSTEM_STATE_UTIL_H_
#define ANDROID_WEBVIEW_BROWSER_METRICS_SYSTEM_STATE_UTIL_H_

#include <optional>
#include <string_view>

namespace android_webview {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. See MultipleUserProfilesState in
// enums.xml.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.android_webview
enum class MultipleUserProfilesState {
  kUnknown = 0,
  kSingleProfile = 1,
  kMultipleProfiles = 2,
  kMaxValue = kMultipleProfiles,
};

// Returns whether there are multiple user profiles.
MultipleUserProfilesState GetMultipleUserProfilesState();

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. See PrimaryCpuAbiBitness in
// enums.xml.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.android_webview
enum class PrimaryCpuAbiBitness {
  kUnknown = 0,
  k32bit = 1,
  k64bit = 2,
  kMaxValue = k64bit,
};

PrimaryCpuAbiBitness GetPrimaryCpuAbiBitness();

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. See AgsaProcessName in enums.xml.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.android_webview
enum class AgsaProcessName {
  kGoogleApp = 0,
  kSearch = 1,
  kInteractor = 2,
  kOther = 3,
  kMaxValue = kOther,
};

// Returns the AGSA process name enum if the host app is AGSA, otherwise
// nullopt.
std::optional<AgsaProcessName> GetAgsaProcessNameEnum();

namespace internal {
// Maps the full process name string to an AgsaProcessName enum value.
AgsaProcessName GetAgsaProcessNameEnumImpl(std::string_view process_name);
}  // namespace internal

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_METRICS_SYSTEM_STATE_UTIL_H_
