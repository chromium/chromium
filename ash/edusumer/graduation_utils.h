// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_EDUSUMER_GRADUATION_UTILS_H_
#define ASH_EDUSUMER_GRADUATION_UTILS_H_

#include "ash/ash_export.h"

class PrefService;

namespace ash::graduation {
// Returns true if the Graduation policy has an upcoming enablement change, i.e.
// the policy has a future start date, a future end date, or future start and
// end dates.
// If the enablement period has passed, or if the policy is already enabled with
// no end date, false is returned.
ASH_EXPORT bool HasUpcomingGraduationEnablementChange(
    PrefService* pref_service);

// Checks Graduation eligibility by reading the kGraduationEnablementStatus pref
// and determining if the Graduation app should be available.
ASH_EXPORT bool IsEligibleForGraduation(PrefService* pref_service);
}  // namespace ash::graduation

#endif  // ASH_EDUSUMER_GRADUATION_UTILS_H_
