// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/sync/base/features.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_service_impl.h"
#include "components/sync/driver/sync_user_settings.h"
#include "content/public/test/browser_test.h"

namespace {

// Test fixture running in Ash, with LacrosPrimary enabled.
class LacrosPrimaryAshSyncTest : public SyncTest {
 public:
  LacrosPrimaryAshSyncTest() : SyncTest(SINGLE_CLIENT) {
    feature_list_.InitWithFeatures(
        {ash::features::kLacrosSupport, ash::features::kLacrosPrimary}, {});
  }
  ~LacrosPrimaryAshSyncTest() override = default;

 private:
  base::test::ScopedFeatureList feature_list_;
};

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

IN_PROC_BROWSER_TEST_F(LacrosPrimaryAshSyncTest, AshSyncsAllTypes) {
  // With LacrosPrimary (but not LacrosOnly), Sync in the Ash browser is still
  // enabled.
  ASSERT_TRUE(crosapi::browser_util::IsAshBrowserSyncEnabled());

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  syncer::SyncService* sync_service = GetSyncService(0);
  syncer::SyncUserSettings* user_settings = sync_service->GetUserSettings();

  // Both browser types and OS types should be selectable in Ash.
  // NOTE: For now, while Lacros is in development, we check the full set
  // equality here. Longer term, it's probably sufficient (and more robust) to
  // just spot-check a few "representative" types.

  // All the browser types should be selectable in Ash, except for Apps and
  // WifiConfigurations which are part of the selectable *OS* types.
  EXPECT_EQ(
      user_settings->GetRegisteredSelectableTypes(),
      syncer::UserSelectableTypeSet(syncer::UserSelectableType::kBookmarks,
                                    syncer::UserSelectableType::kPreferences,
                                    syncer::UserSelectableType::kPasswords,
                                    syncer::UserSelectableType::kAutofill,
                                    syncer::UserSelectableType::kThemes,
                                    syncer::UserSelectableType::kHistory,
                                    syncer::UserSelectableType::kExtensions,
                                    syncer::UserSelectableType::kReadingList,
                                    syncer::UserSelectableType::kTabs))
      << "Actual: "
      << syncer::UserSelectableTypeSetToString(
             user_settings->GetRegisteredSelectableTypes());

  // All the OS types should be selectable in Ash too.
  EXPECT_EQ(user_settings->GetRegisteredSelectableOsTypes(),
            syncer::UserSelectableOsTypeSet(
                syncer::UserSelectableOsType::kOsApps,
                syncer::UserSelectableOsType::kOsPreferences,
                syncer::UserSelectableOsType::kOsWifiConfigurations))
      << "Actual: "
      << syncer::UserSelectableOsTypeSetToString(
             user_settings->GetRegisteredSelectableOsTypes());

  syncer::ModelTypeSet expected_active_types(
      syncer::BOOKMARKS, syncer::PREFERENCES, syncer::PASSWORDS,
      syncer::AUTOFILL_PROFILE, syncer::AUTOFILL, syncer::AUTOFILL_WALLET_DATA,
      syncer::AUTOFILL_WALLET_METADATA, syncer::AUTOFILL_WALLET_OFFER,
      syncer::THEMES, syncer::EXTENSIONS, syncer::SEARCH_ENGINES,
      syncer::SESSIONS, syncer::APPS, syncer::APP_SETTINGS,
      syncer::EXTENSION_SETTINGS, syncer::HISTORY_DELETE_DIRECTIVES,
      syncer::DICTIONARY, syncer::DEVICE_INFO, syncer::PRIORITY_PREFERENCES,
      syncer::APP_LIST, syncer::ARC_PACKAGE, syncer::PRINTERS,
      syncer::READING_LIST, syncer::USER_EVENTS, syncer::USER_CONSENTS,
      syncer::SEND_TAB_TO_SELF, syncer::SECURITY_EVENTS,
      syncer::WIFI_CONFIGURATIONS, syncer::OS_PREFERENCES,
      syncer::OS_PRIORITY_PREFERENCES, syncer::SHARING_MESSAGE,
      syncer::WORKSPACE_DESK, syncer::PROXY_TABS, syncer::NIGORI);
  if (base::FeatureList::IsEnabled(syncer::kSyncEnableHistoryDataType)) {
    expected_active_types.Put(syncer::HISTORY);
  } else {
    expected_active_types.Put(syncer::TYPED_URLS);
  }

  // All of the model types, both browser and OS, should be active.
  EXPECT_EQ(sync_service->GetActiveDataTypes(), expected_active_types);
}

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
                syncer::UserSelectableOsType::kOsApps,
                syncer::UserSelectableOsType::kOsPreferences,
                syncer::UserSelectableOsType::kOsWifiConfigurations))
      << "Actual: "
      << syncer::UserSelectableOsTypeSetToString(
             user_settings->GetRegisteredSelectableOsTypes());

  // Only OS ModelTypes should be active
  EXPECT_EQ(
      sync_service->GetActiveDataTypes(),
      syncer::ModelTypeSet(
          syncer::DEVICE_INFO, syncer::APP_LIST, syncer::ARC_PACKAGE,
          syncer::PRINTERS, syncer::USER_CONSENTS, syncer::WIFI_CONFIGURATIONS,
          syncer::OS_PREFERENCES, syncer::OS_PRIORITY_PREFERENCES,
          syncer::WORKSPACE_DESK, syncer::NIGORI));
}

}  // namespace
