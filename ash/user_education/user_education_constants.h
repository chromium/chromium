// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_USER_EDUCATION_USER_EDUCATION_CONSTANTS_H_
#define ASH_USER_EDUCATION_USER_EDUCATION_CONSTANTS_H_

#include "ash/ash_export.h"
#include "ui/base/interaction/element_identifier.h"

namespace ash {

// Element IDs -----------------------------------------------------------------

// Uniquely identifies an element corresponding to the Explore app for user
// education. Note that this may be used in multiple contexts (e.g. app window,
// launcher, shelf, etc.), so care must be taken to use the desired context when
// looking up the associated element.
DECLARE_EXPORTED_ELEMENT_IDENTIFIER_VALUE(ASH_EXPORT, kExploreAppElementId);

// Uniquely identifies the `HoldingSpaceTray` for user education.
DECLARE_EXPORTED_ELEMENT_IDENTIFIER_VALUE(ASH_EXPORT,
                                          kHoldingSpaceTrayElementId);

// Uniquely identifies the `HomeButton` for user education.
DECLARE_EXPORTED_ELEMENT_IDENTIFIER_VALUE(ASH_EXPORT, kHomeButtonElementId);

// Uniquely identifies the `SearchBoxView` for user education.
DECLARE_EXPORTED_ELEMENT_IDENTIFIER_VALUE(ASH_EXPORT, kSearchBoxViewElementId);

// Uniquely identifies an element corresponding to the Settings app for user
// education. Note that this may be used in multiple contexts (e.g. app window,
// launcher, shelf, etc.), so care must be taken to use the desired context when
// looking up the associated element.
DECLARE_EXPORTED_ELEMENT_IDENTIFIER_VALUE(ASH_EXPORT, kSettingsAppElementId);

// Uniquely identifies the `ShelfView` for user education.
DECLARE_EXPORTED_ELEMENT_IDENTIFIER_VALUE(ASH_EXPORT, kShelfViewElementId);

// Uniquely identifies the `UnifiedSystemTray` for user education.
DECLARE_EXPORTED_ELEMENT_IDENTIFIER_VALUE(ASH_EXPORT,
                                          kUnifiedSystemTrayElementId);

// Element Names ---------------------------------------------------------------

// Name which may be set for the `HomeButton` during an interaction sequence to
// uniquely identify a particular instance.
constexpr char kHomeButtonElementName[] = "kHomeButtonElementName";

// Name which may be set for the `UnifiedSystemTray` during an interaction
// sequence to uniquely identify a particular instance.
constexpr char kUnifiedSystemTrayElementName[] =
    "kUnifiedSystemTrayElementName";

}  // namespace ash

#endif  // ASH_USER_EDUCATION_USER_EDUCATION_CONSTANTS_H_
