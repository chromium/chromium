// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_PREFS_H_
#define ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_PREFS_H_

#include <optional>

#include "ash/public/cpp/ash_public_export.h"
#include "base/functional/callback_forward.h"

class PrefChangeRegistrar;
class PrefRegistrySimple;
class PrefService;

namespace base {
class Time;
}  // namespace base

namespace ash::holding_space_prefs {

// Registers holding space profile preferences to `registry`.
ASH_PUBLIC_EXPORT void RegisterProfilePrefs(PrefRegistrySimple* registry);

// Resets all preferences to their default values.
ASH_PUBLIC_EXPORT void ResetProfilePrefsForTesting(PrefService* prefs);

// Adds `callback` to `registrar` to be invoked on changes to previews enabled.
ASH_PUBLIC_EXPORT void AddPreviewsEnabledChangedCallback(
    PrefChangeRegistrar* registrar,
    base::RepeatingClosure callback);

// Adds `callback` to `registrar` to be invoked on changes to whether the
// suggestions section should be expanded.
ASH_PUBLIC_EXPORT void AddSuggestionsExpandedChangedCallback(
    PrefChangeRegistrar* registrar,
    base::RepeatingClosure callback);

// Adds `callback` to `registrar` to be invoked on changes to time of first add.
ASH_PUBLIC_EXPORT void AddTimeOfFirstAddChangedCallback(
    PrefChangeRegistrar* registrar,
    base::RepeatingClosure callback);

// Adds `callback` to `registrar` to be invoked on changes to time of first pin.
ASH_PUBLIC_EXPORT void AddTimeOfFirstPinChangedCallback(
    PrefChangeRegistrar* registrar,
    base::RepeatingClosure callback);

// Returns whether previews are enabled.
ASH_PUBLIC_EXPORT bool IsPreviewsEnabled(PrefService* prefs);

// Sets whether previews are `enabled`.
ASH_PUBLIC_EXPORT void SetPreviewsEnabled(PrefService* prefs, bool enabled);

// Returns whether suggestions are expanded.
ASH_PUBLIC_EXPORT bool IsSuggestionsExpanded(PrefService* prefs);

// Sets whether suggestions are `expanded`.
ASH_PUBLIC_EXPORT void SetSuggestionsExpanded(PrefService* prefs,
                                              bool expanded);

// Returns the time when a holding space item was first added. Note that if the
// time of first add is unmarked, `std::nullopt` is returned.
ASH_PUBLIC_EXPORT std::optional<base::Time> GetTimeOfFirstAdd(
    PrefService* prefs);

// Marks the time when the first holding space item was added. If the time of
// first add was previously marked, this no-ops and returns false.
ASH_PUBLIC_EXPORT bool MarkTimeOfFirstAdd(PrefService* prefs);

// Returns the time when holding space first became available. Note that if the
// time of first availability is unmarked, `std::nullopt` is returned.
ASH_PUBLIC_EXPORT std::optional<base::Time> GetTimeOfFirstAvailability(
    PrefService* prefs);

// Marks time when holding space first became available. If the time of first
// availability was previously marked, this no-ops and returns false.
ASH_PUBLIC_EXPORT bool MarkTimeOfFirstAvailability(PrefService* prefs);

// Returns the time when holding space was first entered. Note that if the time
// of first entry is unmarked, `std::nullopt` is returned.
ASH_PUBLIC_EXPORT std::optional<base::Time> GetTimeOfFirstEntry(
    PrefService* prefs);

// Marks time when holding space was first entered. If the time of first entry
// was previously marked, this no-ops and returns false.
ASH_PUBLIC_EXPORT bool MarkTimeOfFirstEntry(PrefService* prefs);

// Returns the time when the first pin to holding space occurred. Note that if
// the time of first pin is unmarked, `std::nullopt` is returned.
ASH_PUBLIC_EXPORT std::optional<base::Time> GetTimeOfFirstPin(
    PrefService* prefs);

// Marks time of when the first pin to holding space occurred. If time of first
// pin was previously marked, this no-ops and returns false.
ASH_PUBLIC_EXPORT bool MarkTimeOfFirstPin(PrefService* prefs);

// Returns the time when the Files app chip in the holding space pinned files
// section placeholder was first pressed. Note that if the time of first press
// is unmarked, `std::nullopt` is returned.
ASH_PUBLIC_EXPORT std::optional<base::Time> GetTimeOfFirstFilesAppChipPress(
    PrefService* prefs);

// Marks the time when the Files app chip in the holding space pinned files
// section placeholder was first pressed. If the time of first press was
// previously marked, this no-ops and returns false.
ASH_PUBLIC_EXPORT bool MarkTimeOfFirstFilesAppChipPress(PrefService* prefs);

}  // namespace ash::holding_space_prefs

#endif  // ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_PREFS_H_
