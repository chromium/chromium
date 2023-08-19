// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_METRICS_SYSTEM_STATE_UTIL_H_
#define ANDROID_WEBVIEW_BROWSER_METRICS_SYSTEM_STATE_UTIL_H_

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

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_METRICS_SYSTEM_STATE_UTIL_H_
