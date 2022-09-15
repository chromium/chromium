// Copyright 2022 The Chromium Authors. All rights reserved.
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
  kMaxValue = kVersionButton
};

}  // namespace ash

#endif  // ASH_CONSTANTS_QUICK_SETTINGS_CATALOGS_H_
