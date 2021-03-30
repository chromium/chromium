// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/full_restore/full_restore_prefs.h"

#include "ash/public/cpp/ash_features.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"

namespace chromeos {
namespace full_restore {

// An integer pref to define whether restore apps and web pages on startup.
// Refer to |RestoreOption|.
const char kRestoreAppsAndPagesPrefName[] = "settings.restore_apps_and_pages";

// An integer pref to count how many times the user selected the 'Restore'
// button from the restore notification dialog.
const char kRestoreSelectedCountPrefName[] =
    "full_restore.restore_selected_count";

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  if (!ash::features::IsFullRestoreEnabled())
    return;

  registry->RegisterIntegerPref(
      kRestoreAppsAndPagesPrefName, static_cast<int>(RestoreOption::kAlways),
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);

  registry->RegisterIntegerPref(kRestoreSelectedCountPrefName, 0);
}

bool HasRestorePref(PrefService* prefs) {
  return prefs->HasPrefPath(kRestoreAppsAndPagesPrefName);
}

void SetDefaultRestorePrefIfNecessary(PrefService* prefs) {
  DCHECK(!HasRestorePref(prefs));

  if (user_manager::UserManager::Get()->IsCurrentUserNew() ||
      !prefs->HasPrefPath(prefs::kRestoreOnStartup)) {
    prefs->SetInteger(kRestoreAppsAndPagesPrefName,
                      static_cast<int>(RestoreOption::kAskEveryTime));
    return;
  }

  SessionStartupPref session_startup_pref =
      SessionStartupPref::GetStartupPref(prefs);
  if (session_startup_pref.type == SessionStartupPref::LAST) {
    prefs->SetInteger(kRestoreAppsAndPagesPrefName,
                      static_cast<int>(RestoreOption::kAskEveryTime));
  } else {
    prefs->SetInteger(kRestoreAppsAndPagesPrefName,
                      static_cast<int>(RestoreOption::kDoNotRestore));
  }
}

void UpdateRestorePrefIfNecessary(PrefService* prefs) {
  if (!prefs->HasPrefPath(prefs::kRestoreOnStartup))
    return;

  SessionStartupPref session_startup_pref =
      SessionStartupPref::GetStartupPref(prefs);
  if (session_startup_pref.type == SessionStartupPref::LAST) {
    prefs->SetInteger(kRestoreAppsAndPagesPrefName,
                      static_cast<int>(RestoreOption::kAlways));
  } else {
    prefs->SetInteger(kRestoreAppsAndPagesPrefName,
                      static_cast<int>(RestoreOption::kAskEveryTime));
  }
}

int GetRestoreSelectedCountPref(PrefService* prefs) {
  return prefs->GetInteger(kRestoreSelectedCountPrefName);
}

void SetRestoreSelectedCountPref(PrefService* prefs, int count) {
  prefs->SetInteger(kRestoreSelectedCountPrefName, count);
}

}  // namespace full_restore
}  // namespace chromeos
