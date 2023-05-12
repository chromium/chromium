// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/sync/os_sync_util.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ui/webui/settings/ash/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/pref_names.h"

namespace os_sync_util {
namespace {

// Returns true if the prefs were migrated.
bool MaybeMigratePreferencesForSyncSettingsCategorization(PrefService* prefs) {
  // TODO(crbug.com/1249845): Remove this migration code after 2023-06 (see
  // also crbug.com/1255724).
  bool migrated_this_time = false;

  // Don't migrate more than once.
  if (!prefs->GetBoolean(syncer::prefs::internal::kOsSyncPrefsMigrated)) {
    // OS sync model types get their initial state from the corresponding
    // browser model types.
    bool sync_all =
        prefs->GetBoolean(syncer::prefs::internal::kSyncKeepEverythingSynced);
    prefs->SetBoolean(syncer::prefs::internal::kSyncAllOsTypes, sync_all);

    bool sync_apps = prefs->GetBoolean(syncer::prefs::internal::kSyncApps);
    prefs->SetBoolean(syncer::prefs::internal::kSyncOsApps, sync_apps);

    bool sync_preferences =
        prefs->GetBoolean(syncer::prefs::internal::kSyncPreferences);
    prefs->SetBoolean(syncer::prefs::internal::kSyncOsPreferences,
                      sync_preferences);

    // Wallpaper requires both theme sync (called "Themes & Wallpaper" in sync
    // settings) and app sync (to actually sync the data from the wallpaper
    // app).
    bool sync_wallpaper =
        sync_apps && prefs->GetBoolean(syncer::prefs::internal::kSyncThemes);
    prefs->SetBoolean(ash::settings::prefs::kSyncOsWallpaper, sync_wallpaper);
    prefs->SetBoolean(syncer::prefs::internal::kOsSyncPrefsMigrated, true);
    migrated_this_time = true;
  }

  return migrated_this_time;
}

}  // namespace

void MigrateOsSyncPreferences(PrefService* prefs) {
  bool migrated = MaybeMigratePreferencesForSyncSettingsCategorization(prefs);
  base::UmaHistogramBoolean("ChromeOS.Sync.PreferencesMigrated", migrated);
}

}  // namespace os_sync_util
