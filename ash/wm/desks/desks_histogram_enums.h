// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_DESKS_HISTOGRAM_ENUMS_H_
#define ASH_WM_DESKS_DESKS_HISTOGRAM_ENUMS_H_

namespace ash {

// These values are logged to UMA. Entries should not be renumbered and
// numeric values should never be reused. Please keep in sync with
// DesksCreationRemovalSource in src/tools/metrics/histograms/enums.xml.
enum class DesksCreationRemovalSource {
  kButton = 0,
  kKeyboard = 1,
  kMaxValue = kKeyboard,
};

// These values are logged to UMA. Entries should not be renumbered and
// numeric values should never be reused. Please keep in sync with
// DesksMoveWindowFromActiveDeskSource in
// src/tools/metrics/histograms/enums.xml.
enum class DesksMoveWindowFromActiveDeskSource {
  kDragAndDrop = 0,
  kShortcut = 1,
  kMaxValue = kShortcut,
};

// These values are logged to UMA. Entries should not be renumbered and
// numeric values should never be reused. Please keep in sync with
// DesksSwitchSource in src/tools/metrics/histograms/enums.xml.
enum class DesksSwitchSource {
  kNewDeskShortcut = 0,
  kDeskRemoved = 1,
  kDeskSwitchShortcut = 2,
  kMiniViewButton = 3,
  kWindowActivated = 4,
  kDeskSwitchTouchpad = 5,
  kUserSwitch = 6,
  kMaxValue = kUserSwitch,
};

}  // namespace ash

#endif  // ASH_WM_DESKS_DESKS_HISTOGRAM_ENUMS_H_
