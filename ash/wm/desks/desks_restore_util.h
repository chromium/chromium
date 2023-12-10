// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_DESKS_RESTORE_UTIL_H_
#define ASH_WM_DESKS_DESKS_RESTORE_UTIL_H_

#include "ash/ash_export.h"

class PrefRegistrySimple;

namespace base {
class Clock;
class Time;
}  // namespace base

namespace ash {

namespace desks_restore_util {

// Registers the profile prefs needed for restoring virtual desks.
void RegisterProfilePrefs(PrefRegistrySimple* registry);

// Called when `OnFirstSessionStarted()` is triggered to restore the desks, and
// their names from the primary user's prefs.
void RestorePrimaryUserDesks();

// Called to update the desk names restore prefs for the primary user whenever
// desks count or a desk name changes.
ASH_EXPORT void UpdatePrimaryUserDeskNamesPrefs();

// Called to update the desk guids restore prefs for the primary user whenever
// a desk is created or destroyed.
ASH_EXPORT void UpdatePrimaryUserDeskGuidsPrefs();

// Called to update the desk lacros profile ID associations for the primary user
// whenever it changes.
ASH_EXPORT void UpdatePrimaryUserDeskLacrosProfileIdPrefs();

// Called to update the desk metrics restore prefs for the primary user whenever
// desks count changes, desks order changes or during
// `DesksController::Shutdown()`.
ASH_EXPORT void UpdatePrimaryUserDeskMetricsPrefs();

// Called to update the active desk restore prefs for the primary user whenever
// the primary user switches an active desk.
void UpdatePrimaryUserActiveDeskPrefs(int active_desk_index);

// Returns the time from `g_override_clock_` if it is not nullptr, or time from
// base::Time::Now() otherwise.
const base::Time GetTimeNow();

// Returns the time from GetTimeNow() to Jan 1, 2010 in the local timezeone in
// days as an int. We use Jan 1, 2010 as an arbitrary epoch since it is a
// well-known date in the past.
ASH_EXPORT int GetDaysFromLocalEpoch();

ASH_EXPORT void OverrideClockForTesting(base::Clock* test_clock);

}  // namespace desks_restore_util

}  // namespace ash

#endif  // ASH_WM_DESKS_DESKS_RESTORE_UTIL_H_
