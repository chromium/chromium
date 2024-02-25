// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_INTENT_HELPER_INTENT_CHIP_DISPLAY_PREFS_H_
#define CHROME_BROWSER_APPS_INTENT_HELPER_INTENT_CHIP_DISPLAY_PREFS_H_

#include "url/gurl.h"

class Profile;

// Stores and manages user preferences about how the Intent Chip should be
// displayed for each origin.
class IntentChipDisplayPrefs {
 public:
  // Whether the Intent Chip displays as Expanded (full chip with label
  // text) or collapsed (just an icon).
  enum class ChipState { kExpanded = 0, kCollapsed = 1 };

  // Returns a ChipState indicating whether the Intent Chip should be shown as
  // expanded or collapsed for a given URL. Increments an internal counter to
  // track the number of times the chip has been shown for that URL.
  static ChipState GetChipStateAndIncrementCounter(Profile* profile,
                                                   const GURL& url);

  // Reset the intent chip counter to 0. When this is called, it allows the
  // GetChipStateAndIncrementCounter function will return an Expanded ChipState
  // another 3 times for that |url|.
  static void ResetIntentChipCounter(Profile* profile, const GURL& url);
};

#endif  // CHROME_BROWSER_APPS_INTENT_HELPER_INTENT_CHIP_DISPLAY_PREFS_H_
