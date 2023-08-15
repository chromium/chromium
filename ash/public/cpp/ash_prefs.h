// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_ASH_PREFS_H_
#define ASH_PUBLIC_CPP_ASH_PREFS_H_

#include <string_view>

#include "ash/ash_export.h"

class PrefRegistrySimple;

namespace ash {

// Registers all ash related local state prefs to the given |registry|.
ASH_EXPORT void RegisterLocalStatePrefs(PrefRegistrySimple* registry,
                                        bool for_test = false);

// Register ash related sign-in/user profile prefs to |registry|. When
// |for_test| is true this registers foreign user profile prefs (e.g. chrome
// prefs) as if they are owned by ash. This allows test code to read the pref
// values. |country| should be the permanent country code stored for this
// client in lowercase ISO 3166-1 alpha-2. It can be used to pick country
// specific default values. May be empty in which case country-specific prefs
// may fail to register correctly.
ASH_EXPORT void RegisterSigninProfilePrefs(PrefRegistrySimple* registry,
                                           std::string_view country,
                                           bool for_test = false);
ASH_EXPORT void RegisterUserProfilePrefs(PrefRegistrySimple* registry,
                                         std::string_view country,
                                         bool for_test = false);

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_ASH_PREFS_H_
