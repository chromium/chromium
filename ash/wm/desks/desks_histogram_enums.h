// Copyright 2019 The Chromium Authors
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
  kDesksRestore = 2,
  kLaunchTemplate = 3,
  kDragToNewDeskButton = 4,
  kSaveAndRecall = 5,
  kApi = 6,
  kEnsureDefaultDesk = 7,
  kFloatingWorkspace = 8,
  kMaxValue = kFloatingWorkspace,
};

// These values are logged to UMA. Entries should not be renumbered and
// numeric values should never be reused. Please keep in sync with
// DesksMoveWindowFromActiveDeskSource in
// src/tools/metrics/histograms/enums.xml.
enum class DesksMoveWindowFromActiveDeskSource {
  kDragAndDrop = 0,
  kShortcut = 1,
  kSendToDesk = 2,
  kVisibleOnAllDesks = 3,
  kMaxValue = kVisibleOnAllDesks,
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
  kPersistentDesksBar = 7,
  kLaunchTemplate = 8,
  kIndexedDeskSwitchShortcut = 9,
  kRemovalUndone = 10,
  kApiSwitch = 11,
  kApiLaunch = 12,
  kMaxValue = kApiLaunch,
};

constexpr char kNewDeskHistogramName[] = "Ash.Desks.NewDesk2";
constexpr char kDeskSwitchHistogramName[] = "Ash.Desks.DesksSwitch";

constexpr char kNumberOfCustomNamesHistogramName[] =
    "Ash.Desks.CustomNameCount";
constexpr char kPercentageOfCustomNamesHistogramName[] =
    "Ash.Desks.CustomNamePercentage";
constexpr char kCustomNameCreatedHistogramName[] =
    "Ash.Desks.CustomNameCreated";

}  // namespace ash

#endif  // ASH_WM_DESKS_DESKS_HISTOGRAM_ENUMS_H_
