// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/account_manager/account_apps_availability.h"

#include "ash/constants/ash_features.h"
#include "base/feature_list.h"

namespace ash {

AccountAppsAvailability::AccountAppsAvailability() = default;
AccountAppsAvailability::~AccountAppsAvailability() = default;

// static
bool AccountAppsAvailability::IsArcAccountRestrictionsEnabled() {
  return base::FeatureList::IsEnabled(
             chromeos::features::kArcAccountRestrictions) &&
         base::FeatureList::IsEnabled(chromeos::features::kLacrosSupport);
}

}  // namespace ash
