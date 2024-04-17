// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_UTILITY_FOREST_UTIL_H_
#define ASH_UTILITY_FOREST_UTIL_H_

#include "ash/ash_export.h"

namespace ash {

ASH_EXPORT bool IsForestFeatureFlagEnabled();

// Checks for the forest feature. This needs a secret key, unless the active
// user is a google account.
ASH_EXPORT bool IsForestFeatureEnabled();

}  // namespace ash

#endif  // ASH_UTILITY_FOREST_UTIL_H_
