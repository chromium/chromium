// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_EDUSUMER_GRADUATION_UTILS_H_
#define ASH_EDUSUMER_GRADUATION_UTILS_H_

#include "ash/ash_export.h"

class PrefService;

namespace ash::graduation {
// Checks Graduation eligibility by reading the kGraduationEnablementStatus pref
// and determining if the Graduation app should be available.
ASH_EXPORT bool IsEligibleForGraduation(PrefService* pref_service);
}  // namespace ash::graduation

#endif  // ASH_EDUSUMER_GRADUATION_UTILS_H_
