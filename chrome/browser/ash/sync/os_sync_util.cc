// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/sync/os_sync_util.h"

#include "ash/constants/ash_features.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ui/webui/settings/chromeos/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/pref_names.h"

namespace os_sync_util {
namespace {

void MaybeMigratePreferencesForSyncConsentOptional(PrefService* prefs) {
  DCHECK(chromeos::features::IsSyncSettingsCategorizationEnabled());

  if (!ash::features::IsSyncConsentOptionalEnabled()) {
    // Always enable the OS sync if SyncConsentOptional is disabled. Before the
    // SyncConsentOptional launch it's impossible to disable/enable the OS sync
    // from the UI.
    prefs->SetBoolean(syncer::prefs::kOsSyncFeatureEnabled, true);
    return;
  }

  // TODO(https://crbug.com/1246357) Add a migration code here. To handle switch
  // between SyncConsentOptional being disabled and enabled
  prefs->SetBoolean(syncer::prefs::kOsSyncFeatureEnabled, false);
}

// Returns true if the prefs were migrated.
bool MaybeMigratePreferencesForSyncSettingsCategorization(PrefService* prefs) {
  // Migration code can be removed when SyncSettingsCategorization has been
  // fully deployed to stable channel for a couple milestones.
  if (!chromeos::features::IsSyncSettingsCategorizationEnabled()) {
    // SyncSettingsCategorization should be launched before SyncConsentOptional.
    DCHECK(!chromeos::features::IsSyncConsentOptionalEnabled());

    // Reset the migration flag because this might be a rollback of the feature.
    // We want migration to happen again when the feature is enabled.
    prefs->SetBoolean(syncer::prefs::kOsSyncPrefsMigrated, false);
    // Reset the OS sync pref to its default state, such that we get the same
    // migration behavior next time SyncSettingsCategorization is enabled.
    prefs->SetBoolean(syncer::prefs::kOsSyncFeatureEnabled, false);

    prefs->ClearPref(syncer::prefs::kSyncAllOsTypes);
    prefs->ClearPref(syncer::prefs::kSyncOsApps);
    prefs->ClearPref(syncer::prefs::kSyncOsPreferences);
    prefs->ClearPref(chromeos::settings::prefs::kSyncOsWallpaper);

    return false;
  }

  bool migrated_this_time = false;

  // Don't migrate more than once.
  if (!prefs->GetBoolean(syncer::prefs::kOsSyncPrefsMigrated)) {
    // OS sync model types get their initial state from the corresponding
    // browser model types.
    bool sync_all = prefs->GetBoolean(syncer::prefs::kSyncKeepEverythingSynced);
    prefs->SetBoolean(syncer::prefs::kSyncAllOsTypes, sync_all);

    bool sync_apps = prefs->GetBoolean(syncer::prefs::kSyncApps);
    prefs->SetBoolean(syncer::prefs::kSyncOsApps, sync_apps);

    bool sync_preferences = prefs->GetBoolean(syncer::prefs::kSyncPreferences);
    prefs->SetBoolean(syncer::prefs::kSyncOsPreferences, sync_preferences);

    // Wallpaper requires both theme sync (called "Themes & Wallpaper" in sync
    // settings) and app sync (to actually sync the data from the wallpaper
    // app).
    bool sync_wallpaper =
        sync_apps && prefs->GetBoolean(syncer::prefs::kSyncThemes);
    prefs->SetBoolean(chromeos::settings::prefs::kSyncOsWallpaper,
                      sync_wallpaper);
    prefs->SetBoolean(syncer::prefs::kOsSyncPrefsMigrated, true);
    migrated_this_time = true;
  }

  MaybeMigratePreferencesForSyncConsentOptional(prefs);

  return migrated_this_time;
}

}  // namespace

void MigrateOsSyncPreferences(PrefService* prefs) {
  bool migrated = MaybeMigratePreferencesForSyncSettingsCategorization(prefs);
  base::UmaHistogramBoolean("ChromeOS.Sync.PreferencesMigrated", migrated);
}

}  // namespace os_sync_util
