// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASH_ELEMENT_IDENTIFIERS_H_
#define ASH_ASH_ELEMENT_IDENTIFIERS_H_

#include "ash/ash_export.h"
#include "ui/base/interaction/element_identifier.h"

namespace ash {

// Element IDs -----------------------------------------------------------------
// Please keep this list alphabetized.

// Uniquely identifies the app list bubble view (the clamshell mode launcher).
DECLARE_EXPORTED_ELEMENT_IDENTIFIER_VALUE(ASH_EXPORT,
                                          kAppListBubbleViewElementId);

DECLARE_EXPORTED_ELEMENT_IDENTIFIER_VALUE(ASH_EXPORT,
                                          kAssistantDialogPlateElementId);

DECLARE_EXPORTED_ELEMENT_IDENTIFIER_VALUE(
    ASH_EXPORT,
    kBluetoothDetailedViewPairNewDeviceElementId);
DECLARE_EXPORTED_ELEMENT_IDENTIFIER_VALUE(
    ASH_EXPORT,
    kBluetoothDetailedViewToggleElementId);
DECLARE_EXPORTED_ELEMENT_IDENTIFIER_VALUE(ASH_EXPORT,
                                          kBluetoothFeatureTileToggleElementId);
DECLARE_EXPORTED_ELEMENT_IDENTIFIER_VALUE(
    ASH_EXPORT,
    kBluetoothFeatureTileDrillInArrowElementId);

DECLARE_EXPORTED_ELEMENT_IDENTIFIER_VALUE(ASH_EXPORT, kCalendarViewElementId);

DECLARE_EXPORTED_ELEMENT_IDENTIFIER_VALUE(
    ASH_EXPORT,
    kCellularManagedSimLockNotificationElementId);

DECLARE_EXPORTED_ELEMENT_IDENTIFIER_VALUE(ASH_EXPORT, kEmojiPickerElementId);

DECLARE_EXPORTED_ELEMENT_IDENTIFIER_VALUE(ASH_EXPORT, kEnterpriseManagedView);

// Uniquely identifies an element corresponding to the Explore app. Note that
// this may be used in multiple contexts (e.g. app window, launcher, shelf,
// etc.), so care must be taken to use the desired context when looking up the
// associated element.
DECLARE_EXPORTED_ELEMENT_IDENTIFIER_VALUE(ASH_EXPORT, kExploreAppElementId);

// Uniquely identifies an element corresponding to the Files app.
DECLARE_EXPORTED_ELEMENT_IDENTIFIER_VALUE(ASH_EXPORT, kFilesAppElementId);

// Uniquely identifies the `HoldingSpaceTray`.
DECLARE_EXPORTED_ELEMENT_IDENTIFIER_VALUE(ASH_EXPORT,
                                          kHoldingSpaceTrayElementId);

// Uniquely identifies the home (launcher) button.
DECLARE_EXPORTED_ELEMENT_IDENTIFIER_VALUE(ASH_EXPORT, kHomeButtonElementId);

// Uniquely identifies the toggle of the hotspot detailed view.
DECLARE_EXPORTED_ELEMENT_IDENTIFIER_VALUE(ASH_EXPORT,
                                          kHotspotDetailedViewToggleElementId);

// Uniquely identifies the drill in button in the hotspot tile.
DECLARE_EXPORTED_ELEMENT_IDENTIFIER_VALUE(
    ASH_EXPORT,
    kHotspotFeatureTileDrillInArrowElementId);

// Uniquely identifies the `LoginUserView`.
DECLARE_EXPORTED_ELEMENT_IDENTIFIER_VALUE(ASH_EXPORT, kLoginUserViewElementId);

DECLARE_EXPORTED_ELEMENT_IDENTIFIER_VALUE(ASH_EXPORT, kNetworkAddEsimElementId);

DECLARE_EXPORTED_ELEMENT_IDENTIFIER_VALUE(
    ASH_EXPORT,
    kNetworkDetailedViewMobileDataToggleElementId);

// Uniquely identifies the drill-in arrow of the network feature tile.
DECLARE_EXPORTED_ELEMENT_IDENTIFIER_VALUE(
    ASH_EXPORT,
    kNetworkFeatureTileDrillInArrowElementId);

// Uniquely identifies the `LoginUserView`.
DECLARE_EXPORTED_ELEMENT_IDENTIFIER_VALUE(ASH_EXPORT, kLoginUserViewElementId);

DECLARE_EXPORTED_ELEMENT_IDENTIFIER_VALUE(ASH_EXPORT,
                                          kOverviewDeskBarElementId);

DECLARE_EXPORTED_ELEMENT_IDENTIFIER_VALUE(
    ASH_EXPORT,
    kOverviewDeskBarNewDeskButtonElementId);

//
DECLARE_EXPORTED_ELEMENT_IDENTIFIER_VALUE(
    ASH_EXPORT,
    kNetworkFeatureTileDrillInArrowElementId);

DECLARE_EXPORTED_ELEMENT_IDENTIFIER_VALUE(ASH_EXPORT,
                                          kOverviewDeskBarElementId);

DECLARE_EXPORTED_ELEMENT_IDENTIFIER_VALUE(
    ASH_EXPORT,
    kOverviewDeskBarNewDeskButtonElementId);

// Identifies the overall Picker window UI.
// See go/palette-next for more information about Picker.
DECLARE_EXPORTED_ELEMENT_IDENTIFIER_VALUE(ASH_EXPORT, kPickerElementId);

// Identifies the emoji bar of Picker.
DECLARE_EXPORTED_ELEMENT_IDENTIFIER_VALUE(ASH_EXPORT, kPickerEmojiBarElementId);

// Identifies an emoji result in the emoji bar of Picker.
DECLARE_EXPORTED_ELEMENT_IDENTIFIER_VALUE(ASH_EXPORT,
                                          kPickerEmojiItemElementId);

// Identifies the 'GIF' button in the emoji bar of Picker.
DECLARE_EXPORTED_ELEMENT_IDENTIFIER_VALUE(ASH_EXPORT, kPickerGifElementId);

// Identifies the 'more emojis' button in the emoji bar of Picker.
DECLARE_EXPORTED_ELEMENT_IDENTIFIER_VALUE(ASH_EXPORT,
                                          kPickerMoreEmojisElementId);

// Identifies the textfield that implements the Picker search field.
DECLARE_EXPORTED_ELEMENT_IDENTIFIER_VALUE(ASH_EXPORT,
                                          kPickerSearchFieldTextfieldElementId);

// Identifies the search results page of Picker.
DECLARE_EXPORTED_ELEMENT_IDENTIFIER_VALUE(ASH_EXPORT,
                                          kPickerSearchResultsPageElementId);

// Identifies a list result in the search results page of Picker.
DECLARE_EXPORTED_ELEMENT_IDENTIFIER_VALUE(
    ASH_EXPORT,
    kPickerSearchResultsListItemElementId);

DECLARE_EXPORTED_ELEMENT_IDENTIFIER_VALUE(
    ASH_EXPORT,
    kQuickSettingsAudioDetailedViewAudioSettingsButtonElementId);
DECLARE_EXPORTED_ELEMENT_IDENTIFIER_VALUE(
    ASH_EXPORT,
    kQuickSettingsAudioDetailedViewButtonElementId);
DECLARE_EXPORTED_ELEMENT_IDENTIFIER_VALUE(
    ASH_EXPORT,
    kQuickSettingsSettingsButtonElementId);
DECLARE_EXPORTED_ELEMENT_IDENTIFIER_VALUE(ASH_EXPORT,
                                          kQuickSettingsViewElementId);

// Uniquely identifies the `SearchBoxView`.
DECLARE_EXPORTED_ELEMENT_IDENTIFIER_VALUE(ASH_EXPORT, kSearchBoxViewElementId);

// Uniquely identifies an element corresponding to the Settings app. Note that
// this may be used in multiple contexts (e.g. app window, launcher, shelf,
// etc.), so care must be taken to use the desired context when looking up the
// associated element.
DECLARE_EXPORTED_ELEMENT_IDENTIFIER_VALUE(ASH_EXPORT, kSettingsAppElementId);

// Uniquely identifies the `ShelfView`.
DECLARE_EXPORTED_ELEMENT_IDENTIFIER_VALUE(ASH_EXPORT, kShelfViewElementId);

// Uniquely identifies the `UnifiedSystemTray`.
DECLARE_EXPORTED_ELEMENT_IDENTIFIER_VALUE(ASH_EXPORT,
                                          kUnifiedSystemTrayElementId);

// Uniquely identifies the `WelcomeTourDialog` for user education.
DECLARE_EXPORTED_ELEMENT_IDENTIFIER_VALUE(ASH_EXPORT,
                                          kWelcomeTourDialogElementId);

// Element Names ---------------------------------------------------------------
// Please keep this list alphabetized.

// Name which may be set for the `HomeButton` during an interaction sequence to
// uniquely identify a particular instance.
inline constexpr char kHomeButtonElementName[] = "kHomeButtonElementName";

// Name which may be set for the `UnifiedSystemTray` during an interaction
// sequence to uniquely identify a particular instance.
inline constexpr char kUnifiedSystemTrayElementName[] =
    "kUnifiedSystemTrayElementName";

}  // namespace ash

#endif  // ASH_ASH_ELEMENT_IDENTIFIERS_H_
