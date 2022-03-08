// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FLAGS_ANDROID_CHROME_SESSION_STATE_H_
#define CHROME_BROWSER_FLAGS_ANDROID_CHROME_SESSION_STATE_H_

#include <jni.h>

#include "base/feature_list.h"

namespace chrome {
namespace android {

// TODO(b/182286787): A/B experiment monitoring session/activity resume order.
extern const base::Feature kFixedUmaSessionResumeOrder;

enum CustomTabsVisibilityHistogram {
  VISIBLE_CUSTOM_TAB,
  VISIBLE_CHROME_TAB,
  NO_VISIBLE_TAB,
  kMaxValue = NO_VISIBLE_TAB,
};

// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.flags
enum class ActivityType {
  kTabbed,
  kCustomTab,
  kTrustedWebActivity,
  kWebapp,
  kWebApk,
  kUnknown,
  kMaxValue = kUnknown,
};

// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.flags
enum class DarkModeState {
  kUnknown,
  // Both system and browser are in dark mode.
  kDarkModeSystem,
  // Browser is in dark mode, system is not/cannot be determined.
  kDarkModeApp,
  // Both system and browser are in light mode.
  kLightModeSystem,
  // Browser is in light mode, system is not/cannot be determined.
  kLightModeApp,
  kMaxValue = kLightModeApp,
};

CustomTabsVisibilityHistogram GetCustomTabsVisibleValue(ActivityType type);

// Sets the raw underlying activity type without triggering any of the usual
// response.  Used for testing.
void SetInitialActivityTypeForTesting(ActivityType type);

// Sets the activity type and emits associated metrics as needed.
void SetActivityType(ActivityType type);

// Returns the current activity type.
ActivityType GetActivityType();

DarkModeState GetDarkModeState();

bool GetIsInMultiWindowModeValue();

// Helper to emit Browser/CCT activity type.
void EmitActivityTypeHistograms(ActivityType type);

}  // namespace android
}  // namespace chrome

#endif  // CHROME_BROWSER_FLAGS_ANDROID_CHROME_SESSION_STATE_H_
