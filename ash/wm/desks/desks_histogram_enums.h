// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_DESKS_HISTOGRAM_ENUMS_H_
#define ASH_WM_DESKS_DESKS_HISTOGRAM_ENUMS_H_

namespace ash {

// The enums below are used with UMA. Entries should not be renumbered and
// numeric values should never be reused.

// Please keep in sync with the matching enum in
// tools/metrics/histograms/metadata/ash/enums.xml
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
  kCoral = 10,
  kMaxValue = kCoral,
};

// Please keep in sync with the matching enum in
// tools/metrics/histograms/metadata/ash/enums.xml
enum class DesksMoveWindowFromActiveDeskSource {
  kDragAndDrop = 0,
  kShortcut = 1,
  kSendToDesk = 2,
  kVisibleOnAllDesks = 3,
  kCoral = 4,
  kMaxValue = kCoral,
};

// Please keep in sync with the matching enum in
// tools/metrics/histograms/metadata/ash/enums.xml
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
  kDeskRestored = 16,
  kCoral = 17,
  kMaxValue = kCoral,
};

inline constexpr char kNewDeskHistogramName[] = "Ash.Desks.NewDesk2";
inline constexpr char kDeskSwitchHistogramName[] = "Ash.Desks.DesksSwitch";
inline constexpr char kRemoveDeskHistogramName[] = "Ash.Desks.RemoveDesk";

inline constexpr char kNumberOfCustomNamesHistogramName[] =
    "Ash.Desks.CustomNameCount";
inline constexpr char kPercentageOfCustomNamesHistogramName[] =
    "Ash.Desks.CustomNamePercentage";
inline constexpr char kCustomNameCreatedHistogramName[] =
    "Ash.Desks.CustomNameCreated";

inline constexpr char kDeskButtonPressesHistogramName[] =
    "Ash.Desks.DeskButton.Presses";

inline constexpr char kDeskButtonDeskBarActivateDeskHistogramName[] =
    "Ash.Desks.DeskButton.BarAction.ActivateDesk";
inline constexpr char kDeskButtonDeskBarCloseDeskHistogramName[] =
    "Ash.Desks.DeskButton.BarAction.CloseDesk";
inline constexpr char kDeskButtonDeskBarCombineDesksHistogramName[] =
    "Ash.Desks.DeskButton.BarAction.CombineDesks";
inline constexpr char kDeskButtonDeskBarNewDeskHistogramName[] =
    "Ash.Desks.DeskButton.BarAction.NewDesk";
inline constexpr char kDeskButtonDeskBarOpenContextMenuHistogramName[] =
    "Ash.Desks.DeskButton.BarAction.OpenContextMenu";
inline constexpr char kDeskButtonDeskBarOpenLibraryHistogramName[] =
    "Ash.Desks.DeskButton.BarAction.OpenLibrary";
inline constexpr char kDeskButtonDeskBarRenameDeskHistogramName[] =
    "Ash.Desks.DeskButton.BarAction.RenameDesk";
inline constexpr char kDeskButtonDeskBarReorderDeskHistogramName[] =
    "Ash.Desks.DeskButton.BarAction.ReorderDesk";

inline constexpr char kOverviewDeskBarActivateDeskHistogramName[] =
    "Ash.Desks.Overview.BarAction.ActivateDesk";
inline constexpr char kOverviewDeskBarCloseDeskHistogramName[] =
    "Ash.Desks.Overview.BarAction.CloseDesk";
inline constexpr char kOverviewDeskBarCombineDesksHistogramName[] =
    "Ash.Desks.Overview.BarAction.CombineDesks";
inline constexpr char kOverviewDeskBarNewDeskHistogramName[] =
    "Ash.Desks.Overview.BarAction.NewDesk";
inline constexpr char kOverviewDeskBarOpenContextMenuHistogramName[] =
    "Ash.Desks.Overview.BarAction.OpenContextMenu";
inline constexpr char kOverviewDeskBarOpenLibraryHistogramName[] =
    "Ash.Desks.Overview.BarAction.OpenLibrary";
inline constexpr char kOverviewDeskBarRenameDeskHistogramName[] =
    "Ash.Desks.Overview.BarAction.RenameDesk";
inline constexpr char kOverviewDeskBarReorderDeskHistogramName[] =
    "Ash.Desks.Overview.BarAction.ReorderDesk";

inline constexpr char kDeskSwitchScreenshotResultHistogramName[] =
    "Ash.Desks.DesksSwitchScreenshotResult";

}  // namespace ash

#endif  // ASH_WM_DESKS_DESKS_HISTOGRAM_ENUMS_H_
