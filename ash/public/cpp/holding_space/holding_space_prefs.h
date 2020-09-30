// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_PREFS_H_
#define ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_PREFS_H_

#include "ash/public/cpp/ash_public_export.h"
#include "base/optional.h"

class PrefService;

namespace base {
class Time;
}  // namespace base

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace ash {
namespace holding_space_prefs {

// Registers holding space profile preferences to `registry`.
ASH_PUBLIC_EXPORT void RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry);

// Returns the time when holding space first became available. Note that if the
// time of first availability is unmarked, `base::nullopt` is returned.
ASH_PUBLIC_EXPORT base::Optional<base::Time> GetTimeOfFirstAvailability(
    PrefService* prefs);

// Marks time when holding space first became available. If the time of first
// availability was previously marked, this no-ops and returns false.
ASH_PUBLIC_EXPORT bool MarkTimeOfFirstAvailability(PrefService* prefs);

// Returns the time when holding space was first entered. Note that if the time
// of first entry is unmarked, `base::nullopt` is returned.
ASH_PUBLIC_EXPORT base::Optional<base::Time> GetTimeOfFirstEntry(
    PrefService* prefs);

// Marks time when holding space was first entered. If the time of first entry
// was previously marked, this no-ops and returns false.
ASH_PUBLIC_EXPORT bool MarkTimeOfFirstEntry(PrefService* prefs);

// Returns the time when the first pin to holding space occurred. Note that if
// the time of first pin is unmarked, `base::nullopt` is returned.
ASH_PUBLIC_EXPORT base::Optional<base::Time> GetTimeOfFirstPin(
    PrefService* prefs);

// Marks time of when the first pin to holding space occurred. If time of first
// pin was previously marked, this no-ops and returns false.
ASH_PUBLIC_EXPORT bool MarkTimeOfFirstPin(PrefService* prefs);

}  // namespace holding_space_prefs
}  // namespace ash

#endif  // ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_PREFS_H_
