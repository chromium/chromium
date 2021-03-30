// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/sync/os_sync_util.h"

#include "ash/constants/ash_features.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ui/webui/settings/chromeos/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/pref_names.h"

namespace os_sync_util {
namespace {

// Returns true if the prefs were migrated.
bool MaybeMigratePreferences(PrefService* prefs) {
  // Migration code can be removed when SplitSettingsSync has been fully
  // deployed to stable channel, likely December 2020. When doing this, change
  // the pref kOsSyncFeatureEnabled to default to true and delete the pref
  // kOsSyncPrefsMigrated.
  if (!chromeos::features::IsSplitSettingsSyncEnabled()) {
    // Reset the migration flag because this might be a rollback of the feature.
    // We want migration to happen again when the feature is enabled.
    prefs->SetBoolean(syncer::prefs::kOsSyncPrefsMigrated, false);
    // Reset the OS sync pref to its default state, such that we get the same
    // migration behavior next time SplitSettingsSync is enabled.
    prefs->SetBoolean(syncer::prefs::kOsSyncFeatureEnabled, false);
    return false;
  }

  // Don't migrate more than once.
  if (prefs->GetBoolean(syncer::prefs::kOsSyncPrefsMigrated))
    return false;

  // OS sync model types get their initial state from the corresponding browser
  // model types.
  bool sync_all = prefs->GetBoolean(syncer::prefs::kSyncKeepEverythingSynced);
  prefs->SetBoolean(syncer::prefs::kSyncAllOsTypes, sync_all);

  bool sync_apps = prefs->GetBoolean(syncer::prefs::kSyncApps);
  prefs->SetBoolean(syncer::prefs::kSyncOsApps, sync_apps);

  bool sync_preferences = prefs->GetBoolean(syncer::prefs::kSyncPreferences);
  prefs->SetBoolean(syncer::prefs::kSyncOsPreferences, sync_preferences);

  // Wallpaper requires both theme sync (called "Themes & Wallpaper" in sync
  // settings) and app sync (to actually sync the data from the wallpaper app).
  bool sync_wallpaper =
      sync_apps && prefs->GetBoolean(syncer::prefs::kSyncThemes);
  prefs->SetBoolean(chromeos::settings::prefs::kSyncOsWallpaper,
                    sync_wallpaper);

  // No need to migrate Wi-Fi. There's not a separate OS pref for it.
  bool sync_wifi = prefs->GetBoolean(syncer::prefs::kSyncWifiConfigurations);

  // Enable the OS sync feature if any OS data type is enabled. Otherwise the
  // user would stop syncing a type that they were syncing before.
  if (sync_all || sync_apps || sync_preferences || sync_wallpaper ||
      sync_wifi) {
    prefs->SetBoolean(syncer::prefs::kOsSyncFeatureEnabled, true);
  }

  prefs->SetBoolean(syncer::prefs::kOsSyncPrefsMigrated, true);
  return true;
}

}  // namespace

void MigrateOsSyncPreferences(PrefService* prefs) {
  bool migrated = MaybeMigratePreferences(prefs);
  base::UmaHistogramBoolean("ChromeOS.Sync.PreferencesMigrated", migrated);
}

}  // namespace os_sync_util
