// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_DESKS_RESTORE_UTIL_H_
#define ASH_WM_DESKS_DESKS_RESTORE_UTIL_H_

class PrefRegistrySimple;

namespace ash {

namespace desks_restore_util {

// Registers the profile prefs needed for restoring virtual desks.
void RegisterProfilePrefs(PrefRegistrySimple* registry);

// Called when `OnFirstSessionStarted()` is triggered to restore the desks, and
// their names from the primary user's prefs.
void RestorePrimaryUserDesks();

// Called to update the desks restore prefs for the primary user, whenever a
// change to the desks count or their names is triggered.
void UpdatePrimaryUserDesksPrefs();

// Called to update the active desk restore prefs for the primary user, whenever
// the primary user switches an active desk.
void UpdatePrimaryUserActiveDeskPrefs(int active_desk_index);

}  // namespace desks_restore_util

}  // namespace ash

#endif  // ASH_WM_DESKS_DESKS_RESTORE_UTIL_H_
