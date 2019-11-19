// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_ASH_PREFS_H_
#define ASH_PUBLIC_CPP_ASH_PREFS_H_

#include "ash/ash_export.h"

class PrefRegistrySimple;

namespace ash {

// Registers all ash related local state prefs to the given |registry|.
ASH_EXPORT void RegisterLocalStatePrefs(PrefRegistrySimple* registry,
                                        bool for_test = false);

// Register ash related sign-in/user profile prefs to |registry|. When
// |for_test| is true this registers foreign user profile prefs (e.g. chrome
// prefs) as if they are owned by ash. This allows test code to read the pref
// values.
ASH_EXPORT void RegisterSigninProfilePrefs(PrefRegistrySimple* registry,
                                           bool for_test = false);
ASH_EXPORT void RegisterUserProfilePrefs(PrefRegistrySimple* registry,
                                         bool for_test = false);

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_ASH_PREFS_H_
