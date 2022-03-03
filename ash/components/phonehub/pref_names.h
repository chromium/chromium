// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_PHONEHUB_PREF_NAMES_H_
#define ASH_COMPONENTS_PHONEHUB_PREF_NAMES_H_

namespace ash {
namespace phonehub {
namespace prefs {

extern const char kCameraRollAccessStatus[];
extern const char kNotificationAccessStatus[];
extern const char kNotificationAccessProhibitedReason[];
extern const char kHideOnboardingUi[];
extern const char kIsAwaitingVerifiedHost[];
extern const char kHasDismissedSetupRequiredUi[];
extern const char kNeedsOneTimeNotificationAccessUpdate[];
extern const char kScreenLockStatus[];
extern const char kRecentAppsHistory[];

}  // namespace prefs
}  // namespace phonehub
}  // namespace ash

// TODO(https://crbug.com/1164001): remove after the migration is finished.
namespace chromeos {
namespace phonehub {
namespace prefs = ::ash::phonehub::prefs;
}
}  // namespace chromeos

#endif  // ASH_COMPONENTS_PHONEHUB_PREF_NAMES_H_
