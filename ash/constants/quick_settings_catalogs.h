// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CONSTANTS_QUICK_SETTINGS_CATALOGS_H_
#define ASH_CONSTANTS_QUICK_SETTINGS_CATALOGS_H_

namespace ash {

// A catalog that registers all the buttons on the Quick Settings page. This
// catalog should be kept in sync with the buttons on the Quick Settings page.
// Current values should not be renumbered or removed, because they are recorded
// in histograms (histograms' enums.xml `QsButtonCatalogName`). To deprecate use
// `_DEPRECATED` post-fix on the name.
enum class QsButtonCatalogName {
  kUnknown = 0,
  kSignOutButton = 1,
  kLockButton = 2,
  kPowerButton = 3,
  kSettingsButton = 4,
  kDateViewButton = 5,
  kBatteryButton = 6,
  kManagedButton = 7,
  kAvatarButton = 8,    // To be deprecated
  kCollapseButton = 9,  // To be deprecated
  kFeedBackButton = 10,
  kVersionButton = 11,
  kPowerOffMenuButton = 12,
  kPowerRestartMenuButton = 13,
  kPowerSignoutMenuButton = 14,
  kPowerLockMenuButton = 15,
  kSupervisedButton = 16,
  kEolNoticeButton = 17,
  kPowerEmailMenuButton = 18,
  kExtendedUpdatesNoticeButton = 19,
  kMaxValue = kExtendedUpdatesNoticeButton
};

// A catalog that registers all the features on the Quick Settings page. This
// catalog should be kept in sync with the pods on the Quick Settings page.
// Current values should not be renumbered or removed, because they are recorded
// in histograms (histograms' enums.xml `QsFeatureCatalogName`). To deprecate
// use `_DEPRECATED` post-fix on the name.
enum class QsFeatureCatalogName {
  kUnknown = 0,
  kNetwork = 1,
  kBluetooth = 2,
  kAccessibility = 3,
  kQuietMode = 4,
  kRotationLock = 5,
  kPrivacyScreen = 6,
  kCaptureMode = 7,
  kNearbyShare = 8,
  kNightLight = 9,
  kCast = 10,
  kVPN = 11,
  kIME = 12,
  kLocale = 13,
  kDarkMode = 14,
  kShelfParty_DEPRECATED = 15,
  kAutozoom = 16,
  kHotspot = 17,
  kFocusMode = 18,
  kMaxValue = kFocusMode
};

// A catalog that registers all the sliders on the Quick Settings page (also
// includes the slider bubble which is a separate bubble from the quick settings
// page). This catalog should be kept in sync with the sliders on the Quick
// Settings page. Current values should not be renumbered or removed, because
// they are recorded in histograms (histograms' enums.xml
// `QsSliderCatalogName`). To deprecate use `_DEPRECATED` post-fix on the name.
enum class QsSliderCatalogName {
  kUnknown = 0,
  kVolume = 1,
  kBrightness = 2,
  kMicGain = 3,
  kKeyboardBrightness = 4,
  kMaxValue = kKeyboardBrightness
};

}  // namespace ash

#endif  // ASH_CONSTANTS_QUICK_SETTINGS_CATALOGS_H_
