// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/sync/os_sync_util.h"

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/webui/settings/ash/os_settings_ui.h"
#include "chrome/browser/ui/webui/settings/ash/pref_names.h"
#include "components/sync/base/pref_names.h"
#include "components/sync/base/sync_prefs.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sp = syncer::prefs::internal;
namespace csp = ::ash::settings::prefs;

class OsSyncUtilTest : public testing::Test {
 public:
  OsSyncUtilTest() {
    syncer::SyncPrefs::RegisterProfilePrefs(prefs_.registry());
    ash::settings::OSSettingsUI::RegisterProfilePrefs(prefs_.registry());
  }

  sync_preferences::TestingPrefServiceSyncable prefs_;
};

TEST_F(OsSyncUtilTest, SimpleMigration) {
  os_sync_util::MigrateOsSyncPreferences(&prefs_);
  EXPECT_TRUE(prefs_.GetBoolean(sp::kOsSyncPrefsMigrated));
  EXPECT_TRUE(prefs_.GetBoolean(sp::kSyncAllOsTypes));
}

TEST_F(OsSyncUtilTest, MigrationWithIndividualBrowserTypes) {
  // Customize the browser data types.
  prefs_.SetBoolean(sp::kSyncKeepEverythingSynced, false);
  prefs_.SetBoolean(sp::kSyncApps, true);
  prefs_.SetBoolean(sp::kSyncPreferences, true);
  prefs_.SetBoolean(sp::kSyncThemes, true);

  os_sync_util::MigrateOsSyncPreferences(&prefs_);

  // Equivalent OS types are enabled.
  EXPECT_FALSE(prefs_.GetBoolean(sp::kSyncAllOsTypes));
  EXPECT_TRUE(prefs_.GetBoolean(sp::kSyncOsApps));
  EXPECT_TRUE(prefs_.GetBoolean(sp::kSyncOsPreferences));
  EXPECT_TRUE(prefs_.GetBoolean(csp::kSyncOsWallpaper));
}

TEST_F(OsSyncUtilTest, MigrationForWallpaperRequiresApps) {
  prefs_.SetBoolean(sp::kSyncKeepEverythingSynced, false);
  ASSERT_FALSE(prefs_.GetBoolean(sp::kSyncApps));
  prefs_.SetBoolean(sp::kSyncThemes, true);
  os_sync_util::MigrateOsSyncPreferences(&prefs_);
  EXPECT_FALSE(prefs_.GetBoolean(csp::kSyncOsWallpaper));
}

TEST_F(OsSyncUtilTest, MigrationOnlyHappensOnce) {
  // Do initial migration.
  os_sync_util::MigrateOsSyncPreferences(&prefs_);

  // Customize some browser prefs.
  prefs_.SetBoolean(sp::kSyncKeepEverythingSynced, false);
  prefs_.SetBoolean(sp::kSyncApps, true);
  prefs_.SetBoolean(sp::kSyncPreferences, true);

  // Customize some OS prefs.
  prefs_.SetBoolean(sp::kSyncAllOsTypes, false);
  prefs_.SetBoolean(sp::kSyncOsApps, false);
  prefs_.SetBoolean(sp::kSyncOsPreferences, false);

  // Try to migrate again.
  os_sync_util::MigrateOsSyncPreferences(&prefs_);

  // OS prefs didn't change.
  EXPECT_FALSE(prefs_.GetBoolean(sp::kSyncAllOsTypes));
  EXPECT_FALSE(prefs_.GetBoolean(sp::kSyncOsApps));
  EXPECT_FALSE(prefs_.GetBoolean(sp::kSyncOsPreferences));
}

TEST_F(OsSyncUtilTest, MigrationMetrics) {
  base::HistogramTester histograms;

  // Initial migration.
  os_sync_util::MigrateOsSyncPreferences(&prefs_);

  // Migration recorded.
  histograms.ExpectBucketCount("ChromeOS.Sync.PreferencesMigrated", true, 1);

  // Try to migrate again. This is a no-op.
  os_sync_util::MigrateOsSyncPreferences(&prefs_);

  // Non-migration recorded.
  histograms.ExpectBucketCount("ChromeOS.Sync.PreferencesMigrated", false, 1);
}
