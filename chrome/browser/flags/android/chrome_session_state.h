// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FLAGS_ANDROID_CHROME_SESSION_STATE_H_
#define CHROME_BROWSER_FLAGS_ANDROID_CHROME_SESSION_STATE_H_

#include <jni.h>

namespace chrome {
namespace android {

enum CustomTabsVisibilityHistogram {
  VISIBLE_CUSTOM_TAB,
  VISIBLE_CHROME_TAB,
  CUSTOM_TABS_VISIBILITY_MAX
};

// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.flags
enum class ActivityType {
  kTabbed,
  kCustomTab,
  kTrustedWebActivity,
  kWebapp,
  kWebApk,
  kMaxValue = kWebApk,
};

CustomTabsVisibilityHistogram GetCustomTabsVisibleValue();

ActivityType GetActivityType();

bool GetIsInMultiWindowModeValue();

}  // namespace android
}  // namespace chrome

#endif  // CHROME_BROWSER_FLAGS_ANDROID_CHROME_SESSION_STATE_H_
