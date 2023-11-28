// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_USER_EDUCATION_HOLDING_SPACE_WALLPAPER_NUDGE_HOLDING_SPACE_WALLPAPER_NUDGE_PREFS_H_
#define ASH_USER_EDUCATION_HOLDING_SPACE_WALLPAPER_NUDGE_HOLDING_SPACE_WALLPAPER_NUDGE_PREFS_H_

#include <optional>

#include "ash/ash_export.h"

class PrefRegistrySimple;
class PrefService;

namespace base {
class Time;
}  // namespace base

namespace ash::holding_space_wallpaper_nudge_prefs {

// Returns the time the nudge was last shown. If on the counterfactual arm, it
// will be the last time the nudge would have been shown. If the nudge has never
// been shown, returns `std::nullopt`.
ASH_EXPORT std::optional<base::Time> GetLastTimeNudgeWasShown(
    PrefService* prefs);

// Returns the number of times the nudge has been shown.
ASH_EXPORT uint64_t GetNudgeShownCount(PrefService* prefs);

// Marks that the nudge has been shown. Updates both the count and timestamp.
ASH_EXPORT void MarkNudgeShown(PrefService* prefs);

// Registers the Holding Space wallpaper nudge prefs to the given `registry`.
ASH_EXPORT void RegisterProfilePrefs(PrefRegistrySimple* registry);

}  // namespace ash::holding_space_wallpaper_nudge_prefs

#endif  // ASH_USER_EDUCATION_HOLDING_SPACE_WALLPAPER_NUDGE_HOLDING_SPACE_WALLPAPER_NUDGE_PREFS_H_
