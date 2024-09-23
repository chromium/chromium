// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CONSTANTS_TRAY_BACKGROUND_VIEW_CATALOG_H_
#define ASH_CONSTANTS_TRAY_BACKGROUND_VIEW_CATALOG_H_

namespace ash {

// Usages of shelf pods (TrayBackgroundView).
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. To deprecate an entry, add the
// `_DEPRECATED` postfix. Once you add an entry here, remember to add it in the
// TrayBackgroundViewCatalogName enum in enums.xml.
enum class TrayBackgroundViewCatalogName {
  kTestCatalogName = 0,
  kUnifiedSystem = 1,
  kDateTray = 2,
  kNotificationCenter = 3,
  kOverview = 4,
  kImeMenu = 5,
  kHoldingSpace = 6,
  kScreenCaptureStopRecording = 7,
  kProjectorAnnotation = 8,
  kDictationStatusArea = 9,
  kDictationAccesibilityWindow = 10,
  kSelectToSpeakStatusArea = 11,
  kSelectToSpeakAccessibilityWindow = 12,
  kEche = 13,
  kMediaPlayer = 14,
  kPalette = 15,
  kPhoneHub = 16,
  kLogoutButton = 17,
  kStatusAreaOverflowButton = 18,
  kVirtualKeyboardStatusArea = 19,
  kVirtualKeyboardAccessibilityWindow = 20,
  kWmMode = 21,
  kVideoConferenceTray = 22,
  kFocusMode = 23,
  kPodsOverflow = 24,
  kMouseKeysStatusArea = 25,
  kMaxValue = kMouseKeysStatusArea,
};

}  // namespace ash

#endif  // ASH_CONSTANTS_TRAY_BACKGROUND_VIEW_CATALOG_H_
