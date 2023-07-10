// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_RESTORE_FULL_RESTORE_PREFS_H_
#define CHROME_BROWSER_ASH_APP_RESTORE_FULL_RESTORE_PREFS_H_

class PrefRegistrySimple;
class PrefService;

namespace ash::full_restore {

// Enum that specifies restore options on startup. The values must not be
// changed as they are persisted on disk.
//
// This is used to record histograms, so do not remove or reorder existing
// entries.
enum class RestoreOption {
  kAlways = 1,
  kAskEveryTime = 2,
  kDoNotRestore = 3,

  // Add any new values above this one, and update kMaxValue to the highest
  // enumerator value.
  kMaxValue = kDoNotRestore,
};

// Prefs to define whether the features are enabled by policy.
inline constexpr char kRestoreAppsEnabled[] = "settings.restore_apps_enabled";
inline constexpr char kGhostWindowEnabled[] = "settings.ghost_window_enabled";

// An integer pref to define whether restore apps and web pages on startup.
// Refer to |RestoreOption|.
inline constexpr char kRestoreAppsAndPagesPrefName[] =
    "settings.restore_apps_and_pages";

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
