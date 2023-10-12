// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_USER_EDUCATION_HOLDING_SPACE_TOUR_HOLDING_SPACE_TOUR_PREFS_H_
#define ASH_USER_EDUCATION_HOLDING_SPACE_TOUR_HOLDING_SPACE_TOUR_PREFS_H_

#include "ash/ash_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class PrefRegistrySimple;
class PrefService;

namespace base {
class Time;
}  // namespace base

namespace ash::holding_space_tour_prefs {

// Returns the time the tour was last shown. If the tour has never been shown,
// returns `absl::nullopt`.
ASH_EXPORT absl::optional<base::Time> GetLastTimeTourWasShown(
    PrefService* prefs);

// Returns the number of times the tour has been shown.
ASH_EXPORT uint64_t GetTourShownCount(PrefService* prefs);

// Marks that the tour has been shown. Updates both the count and timestamp.
ASH_EXPORT void MarkTourShown(PrefService* prefs);

// Registers the Holding Space Tour prefs to the given `registry`.
ASH_EXPORT void RegisterProfilePrefs(PrefRegistrySimple* registry);

}  // namespace ash::holding_space_tour_prefs

#endif  // ASH_USER_EDUCATION_HOLDING_SPACE_TOUR_HOLDING_SPACE_TOUR_PREFS_H_
