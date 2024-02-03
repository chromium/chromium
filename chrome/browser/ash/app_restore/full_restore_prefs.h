// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_RESTORE_FULL_RESTORE_PREFS_H_
#define CHROME_BROWSER_ASH_APP_RESTORE_FULL_RESTORE_PREFS_H_

class PrefRegistrySimple;
class PrefService;

namespace ash::full_restore {

// Prefs to define whether the features are enabled by policy.
inline constexpr char kRestoreAppsEnabled[] = "settings.restore_apps_enabled";
inline constexpr char kGhostWindowEnabled[] = "settings.ghost_window_enabled";

// Registers the restore pref |kRestoreAppsAndPagesPrefName|.
void RegisterProfilePrefs(PrefRegistrySimple* registry);

// Returns true if the pref has |kRestoreAppsAndPagesPrefName|. Otherwise,
// return false.
bool HasRestorePref(PrefService* prefs);

// Returns true if the pref has |kRestoreOnStartup|. Otherwise,
// return false.
bool HasSessionStartupPref(PrefService* prefs);

// Returns true if the restore pref doesn't exist or the pref is 'Always' or
// 'Ask every time'. Otherwise, return false for 'Do not restore'.
bool CanPerformRestore(PrefService* prefs);

// Sets the default restore pref |kRestoreAppsAndPagesPrefName| based on the
// current browser restore settings. If it is the first time to run Chrome OS,
// or the browser restore settings doesn't exist, set the restore pref setting
// as |kAskEveryTime|.
//
// This function should be called only when |kRestoreAppsAndPagesPrefName|
// doesn't exist.
void SetDefaultRestorePrefIfNecessary(PrefService* prefs);

// Updates the restore pref |kRestoreAppsAndPagesPrefName| when the browser
// restore settings is synced.
void UpdateRestorePrefIfNecessary(PrefService* prefs);

}  // namespace ash::full_restore

#endif  // CHROME_BROWSER_ASH_APP_RESTORE_FULL_RESTORE_PREFS_H_
