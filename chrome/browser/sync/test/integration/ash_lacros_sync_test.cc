// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/power_bookmarks/core/power_bookmark_features.h"
#include "components/sync/base/features.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_service_impl.h"
#include "components/sync/service/sync_user_settings.h"
#include "content/public/test/browser_test.h"

namespace {

// Test fixture running in Ash, with LacrosOnly enabled.
class LacrosOnlyAshSyncTest : public SyncTest {
 public:
  LacrosOnlyAshSyncTest() : SyncTest(SINGLE_CLIENT) {
    feature_list_.InitWithFeatures(
        {ash::features::kLacrosSupport, ash::features::kLacrosPrimary,
         ash::features::kLacrosOnly},
        {});
  }
  ~LacrosOnlyAshSyncTest() override = default;

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(LacrosOnlyAshSyncTest, AshSyncsOnlyOSTypes) {
  // With LacrosOnly, Sync in the Ash browser is not enabled anymore.
  ASSERT_FALSE(crosapi::browser_util::IsAshBrowserSyncEnabled());

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  syncer::SyncService* sync_service = GetSyncService(0);
  syncer::SyncUserSettings* user_settings = sync_service->GetUserSettings();

  // No browser types should be selectable in Ash now.
  EXPECT_TRUE(user_settings->GetRegisteredSelectableTypes().Empty());

  // But all the OS types should be selectable.
  EXPECT_EQ(user_settings->GetRegisteredSelectableOsTypes(),
            syncer::UserSelectableOsTypeSet(
                {syncer::UserSelectableOsType::kOsApps,
                 syncer::UserSelectableOsType::kOsPreferences,
                 syncer::UserSelectableOsType::kOsWifiConfigurations}))
      << "Actual: "
      << syncer::UserSelectableOsTypeSetToString(
             user_settings->GetRegisteredSelectableOsTypes());

  // Only OS ModelTypes should be active
  EXPECT_EQ(
      sync_service->GetActiveDataTypes(),
      syncer::ModelTypeSet(
          {syncer::DEVICE_INFO, syncer::APP_LIST, syncer::ARC_PACKAGE,
           syncer::PRINTERS, syncer::USER_CONSENTS, syncer::WIFI_CONFIGURATIONS,
           syncer::OS_PREFERENCES, syncer::OS_PRIORITY_PREFERENCES,
           syncer::WORKSPACE_DESK, syncer::NIGORI}));
}

}  // namespace
