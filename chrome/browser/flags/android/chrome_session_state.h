// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FLAGS_ANDROID_CHROME_SESSION_STATE_H_
#define CHROME_BROWSER_FLAGS_ANDROID_CHROME_SESSION_STATE_H_

#include <jni.h>

#include "base/feature_list.h"

class PrefRegistrySimple;
class PrefService;

namespace chrome {
namespace android {

enum CustomTabsVisibilityHistogram {
  VISIBLE_CUSTOM_TAB,
  VISIBLE_CHROME_TAB,
  NO_VISIBLE_TAB,
  kMaxValue = NO_VISIBLE_TAB,
};

// Following enum should always be in sync with ChromeActivityType defined in
// tools/metrics/histograms/metadata/android/enums.xml

// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.flags
enum class ActivityType {
  // Chrome is running as the Chrome Android Browser App (i.e., traditional
  // Chrome).
  kTabbed,

  // Chrome is running embedded in another application as a Custom Tab.
  // See:
  //   - https://developer.chrome.com/docs/android/custom-tabs/
  kCustomTab,

  // Chrome is running as a Trusted Web Activity.
  //
  // See:
  //   - https://developer.chrome.com/docs/android/trusted-web-activity/
  kTrustedWebActivity,

  // Chrome is running as a Web App
  //
  // See
  //   -
  //   https://chromium.googlesource.com/chromium/src/+/HEAD/docs/webapps/README.md
  kWebapp,

  // Chrome is running as a WebAPK.
  //
  // See:
  //   - https://web.dev/webapks/
  //   -
  //   https://chromium.googlesource.com/chromium/src/+/refs/heads/main/chrome/android/webapk/README.md
  kWebApk,

  // Chrome has started running, but no tab has yet become visible (for example:
  // warm-up,
  // FRE, downloads manager shown in response to a notification click, etc).
  kPreFirstTab,

  // Chrome is running embedded in another application as auth-dedicated tab.
  // TODO(b/353517557): Add a link to a developer guide
  kAuthTab,

  kMaxValue = kAuthTab,
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

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. See MultipleUserProfilesState in
// enums.xml.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.flags
enum class MultipleUserProfilesState {
  kUnknown = 0,
  kSingleProfile = 1,
  kMultipleProfiles = 2,
  kMaxValue = kMultipleProfiles,
};

// Returns the CustomTabs.Visible histogram value that corresponde to |type|.
CustomTabsVisibilityHistogram GetCustomTabsVisibleValue(ActivityType type);

// Gets/sets the raw underlying activity type without triggering any additional
// side-effects.
ActivityType GetInitialActivityTypeForTesting();
void SetInitialActivityTypeForTesting(ActivityType type);

// Sets the activity type and emits associated metrics as needed.
void SetActivityType(PrefService* local_state, ActivityType type);

// Returns the current activity type.
ActivityType GetActivityType();

DarkModeState GetDarkModeState();

bool GetIsInMultiWindowModeValue();

// Helper to emit Browser/CCT activity type histograms.
void EmitActivityTypeHistograms(ActivityType type);

// Registers prefs to store the most recent activity type in Local State.
void RegisterActivityTypePrefs(PrefRegistrySimple* registry);

// Retrieves the activity type from |local_state|.
std::optional<chrome::android::ActivityType> GetActivityTypeFromLocalState(
    PrefService* local_state);

// Saves the activity type |value| to |local_state|.
void SaveActivityTypeToLocalState(PrefService* local_state,
                                  chrome::android::ActivityType value);

// Returns whether there are multiple user profiles.
MultipleUserProfilesState GetMultipleUserProfilesState();

}  // namespace android
}  // namespace chrome

#endif  // CHROME_BROWSER_FLAGS_ANDROID_CHROME_SESSION_STATE_H_
