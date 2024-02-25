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

namespace ash {

namespace holding_space_wallpaper_nudge_metrics {
enum class Interaction;
}  // namespace holding_space_wallpaper_nudge_metrics

namespace holding_space_wallpaper_nudge_prefs {

// Returns the time that the user's eligibility was determined.
ASH_EXPORT std::optional<base::Time> GetTimeOfFirstEligibleSession(
    PrefService* prefs);

// Returns the time the nudge was last shown. If on the counterfactual arm, it
// will be the last time the nudge would have been shown. If the nudge has never
// been shown, returns `std::nullopt`.
ASH_EXPORT std::optional<base::Time> GetLastTimeNudgeWasShown(
    PrefService* prefs);

// Returns the number of times the nudge has been shown.
ASH_EXPORT uint64_t GetNudgeShownCount(PrefService* prefs);

// Returns whether the user associated with the given `prefs` is eligible to see
// the nudge. If eligibility has not been determined, returns `std::nullopt`.
// Note that this does not account for rate limiting.
ASH_EXPORT std::optional<bool> GetUserEligibility(PrefService* prefs);

// Marks that the nudge has been shown. Updates both the count and timestamp.
ASH_EXPORT void MarkNudgeShown(PrefService* prefs);

// Marks now as the first session of an eligible user. Returns `true` if it was
// successfully marked.
ASH_EXPORT bool MarkTimeOfFirstEligibleSession(PrefService* prefs);

// Marks now as the first time that a given `interaction` has occurred since the
// beginning of the user's first eligible session. Returns true if it was
// successfully marked.
ASH_EXPORT bool MarkTimeOfFirstInteraction(
    PrefService* prefs,
    holding_space_wallpaper_nudge_metrics::Interaction interaction);

// Registers the Holding Space wallpaper nudge prefs to the given `registry`.
ASH_EXPORT void RegisterProfilePrefs(PrefRegistrySimple* registry);

// Sets whether user should be considered eligible to see the nudge to
// `eligible`. Returns `true` if successfully set, `false` otherwise.
ASH_EXPORT bool SetUserEligibility(PrefService* prefs, bool eligible);

}  // namespace holding_space_wallpaper_nudge_prefs
}  // namespace ash

#endif  // ASH_USER_EDUCATION_HOLDING_SPACE_WALLPAPER_NUDGE_HOLDING_SPACE_WALLPAPER_NUDGE_PREFS_H_
