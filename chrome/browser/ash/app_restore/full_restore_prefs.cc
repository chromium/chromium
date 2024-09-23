// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_restore/full_restore_prefs.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/wm/window_restore/window_restore_util.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace ash::full_restore {

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(kRestoreAppsEnabled, true);
  registry->RegisterBooleanPref(kGhostWindowEnabled, true);

  registry->RegisterIntegerPref(
      prefs::kRestoreAppsAndPagesPrefName,
      static_cast<int>(RestoreOption::kAskEveryTime),
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);

  if (features::IsForestFeatureEnabled()) {
    registry->RegisterBooleanPref(prefs::kShowInformedRestoreOnboarding, true);
    registry->RegisterIntegerPref(prefs::kInformedRestoreNudgeShownCount, 0);
    registry->RegisterTimePref(prefs::kInformedRestoreNudgeLastShown,
                               base::Time());
    registry->RegisterStringPref(prefs::kInformedRestoreLastVersion,
                                 std::string());
  }
}

bool HasRestorePref(PrefService* prefs) {
  return prefs->HasPrefPath(prefs::kRestoreAppsAndPagesPrefName);
}

bool HasSessionStartupPref(PrefService* prefs) {
  return prefs->HasPrefPath(::prefs::kRestoreOnStartup);
}

bool CanPerformRestore(PrefService* prefs) {
  if (!HasRestorePref(prefs))
    return true;

  return static_cast<RestoreOption>(
             prefs->GetInteger(prefs::kRestoreAppsAndPagesPrefName)) !=
         RestoreOption::kDoNotRestore;
}

void SetDefaultRestorePrefIfNecessary(PrefService* prefs) {
  DCHECK(!HasRestorePref(prefs));

  if (!HasSessionStartupPref(prefs)) {
    prefs->SetInteger(prefs::kRestoreAppsAndPagesPrefName,
                      static_cast<int>(RestoreOption::kAskEveryTime));
    return;
  }

  SessionStartupPref session_startup_pref =
      SessionStartupPref::GetStartupPref(prefs);
  if (session_startup_pref.type == SessionStartupPref::LAST) {
    prefs->SetInteger(prefs::kRestoreAppsAndPagesPrefName,
                      static_cast<int>(RestoreOption::kAskEveryTime));
  } else {
    prefs->SetInteger(prefs::kRestoreAppsAndPagesPrefName,
                      static_cast<int>(RestoreOption::kDoNotRestore));
  }
}

void UpdateRestorePrefIfNecessary(PrefService* prefs) {
  if (!prefs->HasPrefPath(::prefs::kRestoreOnStartup)) {
    return;
  }

  SessionStartupPref session_startup_pref =
      SessionStartupPref::GetStartupPref(prefs);
  if (session_startup_pref.ShouldRestoreLastSession()) {
    prefs->SetInteger(prefs::kRestoreAppsAndPagesPrefName,
                      static_cast<int>(RestoreOption::kAlways));
  } else {
    prefs->SetInteger(prefs::kRestoreAppsAndPagesPrefName,
                      static_cast<int>(RestoreOption::kAskEveryTime));
  }
}

}  // namespace ash::full_restore
