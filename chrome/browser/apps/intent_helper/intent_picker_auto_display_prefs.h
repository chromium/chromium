// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_INTENT_HELPER_INTENT_PICKER_AUTO_DISPLAY_PREFS_H_
#define CHROME_BROWSER_APPS_INTENT_HELPER_INTENT_PICKER_AUTO_DISPLAY_PREFS_H_

#include "url/gurl.h"

class Profile;

// Stores and manages user preferences about whether Intent Picker UI should be
// automatically displayed for each origin.
class IntentPickerAutoDisplayPrefs {
 public:
  // The platform selected by the user to handle this URL for devices of tablet
  // form factor.
  enum class Platform { kNone = 0, kArc = 1, kChrome = 2, kMaxValue = kChrome };

  // Whether the Intent Chip displays as Expanded (full chip with label
  // text) or collapsed (just an icon).
  enum class ChipState { kExpanded = 0, kCollapsed = 1 };

  // Returns whether or not a likely |url| has triggered the UI 2+ times without
  // the user engaging.
  static bool ShouldAutoDisplayUi(Profile* profile, const GURL& url);

  // Keep track of the |url| repetitions.
  static void IncrementPickerUICounter(Profile* profile, const GURL& url);

  // Returns a ChipState indicating whether the Intent Chip should be shown as
  // expanded or collapsed for a given URL. Increments an internal counter to
  // track the number of times the chip has been shown for that URL.
  static ChipState GetChipStateAndIncrementCounter(Profile* profile,
                                                   const GURL& url);

  // Reset the intent chip counter to 0. When this is called, it allows the
  // GetChipStateAndIncrementCounter function will return an Expanded ChipState
  // another 3 times for that |url|.
  static void ResetIntentChipCounter(Profile* profile, const GURL& url);

  // Returns the last platform selected by the user to handle |url|.
  // If it has not been checked then it will return |Platform::kNone|
  // for devices of tablet form factor.
  static Platform GetLastUsedPlatformForTablets(Profile* profile,
                                                const GURL& url);

  // Updates the Platform to |platform| for |url| for devices of
  // tablet form factor.
  static void UpdatePlatformForTablets(Profile* profile,
                                       const GURL& url,
                                       Platform platform);
};

#endif  // CHROME_BROWSER_APPS_INTENT_HELPER_INTENT_PICKER_AUTO_DISPLAY_PREFS_H_
