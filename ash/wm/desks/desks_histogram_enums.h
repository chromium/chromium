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
  // TODO(b/291821991): Update this to reflect that there is now more than one
  // way to use a button to create or remove a desk.
  kButton = 0,
  kKeyboard = 1,
  kDesksRestore = 2,
  kLaunchTemplate = 3,
  kDragToNewDeskButton = 4,
  kSaveAndRecall = 5,
  kApi = 6,
  kEnsureDefaultDesk = 7,
  kFloatingWorkspace = 8,
  kDeskButtonDeskBarButton = 9,
  kMaxValue = kDeskButtonDeskBarButton,
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
  // TODO(b/291821991): Update this to reflect that this metric refers only to
  // mini views in the overview desk bar.
  kMiniViewButton = 3,
  kWindowActivated = 4,
  kDeskSwitchTouchpad = 5,
  kUserSwitch = 6,
  // kPersistentDesksBar = 7,  // Deprecated, feature has been deleted.
  kLaunchTemplate = 8,
  kIndexedDeskSwitchShortcut = 9,
  kRemovalUndone = 10,
  kApiSwitch = 11,
  kApiLaunch = 12,
  kDeskButtonSwitchButton = 13,
  kDeskButtonDeskRemoved = 14,
  kDeskButtonMiniViewButton = 15,
  kMaxValue = kDeskButtonMiniViewButton,
};

constexpr char kNewDeskHistogramName[] = "Ash.Desks.NewDesk2";
constexpr char kDeskSwitchHistogramName[] = "Ash.Desks.DesksSwitch";
constexpr char kRemoveDeskHistogramName[] = "Ash.Desks.RemoveDesk";

constexpr char kNumberOfCustomNamesHistogramName[] =
    "Ash.Desks.CustomNameCount";
constexpr char kPercentageOfCustomNamesHistogramName[] =
    "Ash.Desks.CustomNamePercentage";
constexpr char kCustomNameCreatedHistogramName[] =
    "Ash.Desks.CustomNameCreated";

constexpr char kDeskButtonPressesHistogramName[] =
    "Ash.Desks.DeskButton.Presses";

constexpr char kDeskButtonDeskBarActivateDeskHistogramName[] =
    "Ash.Desks.DeskButton.BarAction.ActivateDesk";
constexpr char kDeskButtonDeskBarCloseDeskHistogramName[] =
    "Ash.Desks.DeskButton.BarAction.CloseDesk";
constexpr char kDeskButtonDeskBarCombineDesksHistogramName[] =
    "Ash.Desks.DeskButton.BarAction.CombineDesks";
constexpr char kDeskButtonDeskBarNewDeskHistogramName[] =
    "Ash.Desks.DeskButton.BarAction.NewDesk";
constexpr char kDeskButtonDeskBarOpenLibraryHistogramName[] =
    "Ash.Desks.DeskButton.BarAction.OpenLibrary";
constexpr char kDeskButtonDeskBarRenameDeskHistogramName[] =
    "Ash.Desks.DeskButton.BarAction.RenameDesk";
constexpr char kDeskButtonDeskBarReorderDeskHistogramName[] =
    "Ash.Desks.DeskButton.BarAction.ReorderDesk";

constexpr char kOverviewDeskBarActivateDeskHistogramName[] =
    "Ash.Desks.Overview.BarAction.ActivateDesk";
constexpr char kOverviewDeskBarCloseDeskHistogramName[] =
    "Ash.Desks.Overview.BarAction.CloseDesk";
constexpr char kOverviewDeskBarCombineDesksHistogramName[] =
    "Ash.Desks.Overview.BarAction.CombineDesks";
constexpr char kOverviewDeskBarNewDeskHistogramName[] =
    "Ash.Desks.Overview.BarAction.NewDesk";
constexpr char kOverviewDeskBarOpenLibraryHistogramName[] =
    "Ash.Desks.Overview.BarAction.OpenLibrary";
constexpr char kOverviewDeskBarRenameDeskHistogramName[] =
    "Ash.Desks.Overview.BarAction.RenameDesk";
constexpr char kOverviewDeskBarReorderDeskHistogramName[] =
    "Ash.Desks.Overview.BarAction.ReorderDesk";

}  // namespace ash

#endif  // ASH_WM_DESKS_DESKS_HISTOGRAM_ENUMS_H_
