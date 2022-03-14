// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_INTENT_HELPER_INTENT_PICKER_AUTO_DISPLAY_SERVICE_H_
#define CHROME_BROWSER_APPS_INTENT_HELPER_INTENT_PICKER_AUTO_DISPLAY_SERVICE_H_

#include "base/memory/raw_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "url/gurl.h"

class Profile;

// Stores and manages user preferences about whether Intent Picker UI should be
// automatically displayed for each origin.
class IntentPickerAutoDisplayService : public KeyedService {
 public:
  // The platform selected by the user to handle this URL for devices of tablet
  // form factor.
  enum class Platform { kNone = 0, kArc = 1, kChrome = 2, kMaxValue = kChrome };

  // Whether the Intent Chip displays as Expanded (full chip with label
  // text) or collapsed (just an icon).
  enum class ChipState { kExpanded = 0, kCollapsed = 1 };

  static IntentPickerAutoDisplayService* Get(Profile* profile);

  explicit IntentPickerAutoDisplayService(Profile* profile);
  IntentPickerAutoDisplayService(const IntentPickerAutoDisplayService&) =
      delete;
  IntentPickerAutoDisplayService& operator=(
      const IntentPickerAutoDisplayService&) = delete;

  // Returns whether or not a likely |url| has triggered the UI 2+ times without
  // the user engaging.
  bool ShouldAutoDisplayUi(const GURL& url);

  // Keep track of the |url| repetitions.
  void IncrementPickerUICounter(const GURL& url);

  // Returns a ChipState indicating whether the Intent Chip should be shown as
  // expanded or collapsed for a given URL. Increments an internal counter to
  // track the number of times the chip has been shown for that URL.
  ChipState GetChipStateAndIncrementCounter(const GURL& url);

  // Reset the intent chip counter to 0. When this is called, it allows the
  // GetChipStateAndIncrementCounter function will return an Expanded ChipState
  // another 3 times for that |url|.
  void ResetIntentChipCounter(const GURL& url);

  // Returns the last platform selected by the user to handle |url|.
  // If it has not been checked then it will return |Platform::kNone|
  // for devices of tablet form factor.
  Platform GetLastUsedPlatformForTablets(const GURL& url);

  // Updates the Platform to |platform| for |url| for devices of
  // tablet form factor.
  void UpdatePlatformForTablets(const GURL& url, Platform platform);

 private:
  raw_ptr<Profile> profile_;
};

#endif  // CHROME_BROWSER_APPS_INTENT_HELPER_INTENT_PICKER_AUTO_DISPLAY_SERVICE_H_
