// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_FJORD_OOBE_FJORD_OOBE_UTIL_H_
#define CHROME_BROWSER_ASH_LOGIN_FJORD_OOBE_FJORD_OOBE_UTIL_H_

#include <string>
#include <string_view>

#include "base/containers/fixed_flat_set.h"

namespace ash::fjord_util {

// Returns if the Fjord variant of OOBE should be shown.
bool ShouldShowFjordOobe();

// Returns if the language code is allowlisted for Fjord OOBE.
bool IsAllowlistedLanguage(std::string_view language_code);

const base::fixed_flat_set<std::string_view, 7>&
GetAllowlistedLanguagesForTesting();

}  // namespace ash::fjord_util

#endif  // CHROME_BROWSER_ASH_LOGIN_FJORD_OOBE_FJORD_OOBE_UTIL_H_
