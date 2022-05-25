// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/shelf_prefs.h"
#include "base/json/json_writer.h"
#include "base/notreached.h"
#include "base/values.h"
#include "chrome/browser/sync/test/integration/preferences_helper.h"
#include "chrome/browser/sync/test/integration/sync_settings_categorization_sync_test.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/updated_progress_marker_checker.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/model_type.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_user_settings.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using preferences_helper::ChangeStringPref;
using preferences_helper::GetPrefs;
using testing::Eq;

class SingleClientOsPreferencesSyncTest
    : public SyncSettingsCategorizationSyncTest {
 public:
  SingleClientOsPreferencesSyncTest()
      : SyncSettingsCategorizationSyncTest(SINGLE_CLIENT) {}
  ~SingleClientOsPreferencesSyncTest() override = default;
};

IN_PROC_BROWSER_TEST_F(SingleClientOsPreferencesSyncTest, Sanity) {
  ASSERT_TRUE(chromeos::features::IsSyncSettingsCategorizationEnabled());
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // Shelf alignment is a Chrome OS only preference.
  ChangeStringPref(/*index=*/0, ash::prefs::kShelfAlignment,
                   ash::kShelfAlignmentRight);
  EXPECT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());
  EXPECT_THAT(GetPrefs(/*index=*/0)->GetString(ash::prefs::kShelfAlignment),
              Eq(ash::kShelfAlignmentRight));
}

class SyncCategorizationBaseTest : public SyncTest {
 public:
  SyncCategorizationBaseTest() : SyncTest(SyncTest::SINGLE_CLIENT) {}

 protected:
  static std::string ConvertToSyncedPrefValue(const base::Value& value) {
    std::string result;
    bool success = base::JSONWriter::Write(value, &result);
    DCHECK(success);
    return result;
  }

  sync_pb::PreferenceSpecifics* GetPreferenceSpecifics(
      syncer::ModelType model_type,
      sync_pb::EntitySpecifics& specifics) {
    switch (model_type) {
      case syncer::ModelType::PREFERENCES:
        return specifics.mutable_preference();
      case syncer::ModelType::PRIORITY_PREFERENCES:
        return specifics.mutable_priority_preference()->mutable_preference();
      case syncer::ModelType::OS_PREFERENCES:
        return specifics.mutable_os_preference()->mutable_preference();
      case syncer::ModelType::OS_PRIORITY_PREFERENCES:
        return specifics.mutable_os_priority_preference()->mutable_preference();
      default:
        NOTREACHED();
        return specifics.mutable_preference();
    }
  }
  void InjectPreferenceToFakeServer(syncer::ModelType model_type,
                                    const char* name,
                                    const base::Value& value) {
    sync_pb::EntitySpecifics specifics;
    sync_pb::PreferenceSpecifics* preference_specifics =
        GetPreferenceSpecifics(model_type, specifics);
    preference_specifics->set_name(name);
    preference_specifics->set_value(ConvertToSyncedPrefValue(value));

    GetFakeServer()->InjectEntity(
        syncer::PersistentUniqueClientEntity::CreateFromSpecificsForTesting(
            /*non_unique_name=*/name,
            /*client_tag=*/name, specifics,
            /*creation_time=*/0, /*last_modified_time=*/0));
  }

  const char* const kOsPreferenceKey = ash::prefs::kShelfAutoHideBehavior;
  const base::Value kOsPreferenceNewValue =
      base::Value(ash::kShelfAutoHideBehaviorAlways);
  const char* const kOsPriorityPreferenceKey = ::prefs::kTapToClickEnabled;
  const base::Value kOsPriorityPreferenceNewValue = base::Value(false);
  base::test::ScopedFeatureList features_;
};

class SyncCategorizationEnabledTest : public SyncCategorizationBaseTest {
 public:
  SyncCategorizationEnabledTest() {
    features_.InitAndEnableFeature(ash::features::kSyncSettingsCategorization);
  }
};

class SyncCategorizationDisabledTest : public SyncCategorizationBaseTest {
 public:
  SyncCategorizationDisabledTest() {
    features_.InitAndDisableFeature(ash::features::kSyncSettingsCategorization);
  }
};

// OS preferences should sync from the new clients as both preferences and OS
// preferences.
IN_PROC_BROWSER_TEST_F(SyncCategorizationEnabledTest,
                       OSPreferencesSyncAsBothTypes) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  ASSERT_NE(*GetPrefs(0)->Get(kOsPreferenceKey), kOsPreferenceNewValue);
  ASSERT_NE(*GetPrefs(0)->Get(kOsPriorityPreferenceKey),
            kOsPriorityPreferenceNewValue);

  ASSERT_TRUE(SetupSync());

  GetPrefs(/*index=*/0)->Set(kOsPreferenceKey, kOsPreferenceNewValue);
  GetPrefs(/*index=*/0)
      ->Set(kOsPriorityPreferenceKey, kOsPriorityPreferenceNewValue);

  // OS preferences are syncing both as OS_PREFERENCES and PREFERENCES to
  // support sync to the old clients.
  EXPECT_TRUE(FakeServerPrefMatchesValueChecker(
                  syncer::ModelType::OS_PREFERENCES, kOsPreferenceKey,
                  ConvertToSyncedPrefValue(kOsPreferenceNewValue))
                  .Wait());
  EXPECT_TRUE(FakeServerPrefMatchesValueChecker(
                  syncer::ModelType::PREFERENCES, kOsPreferenceKey,
                  ConvertToSyncedPrefValue(kOsPreferenceNewValue))
                  .Wait());

  // Same with OS priority preferences.
  EXPECT_TRUE(FakeServerPrefMatchesValueChecker(
                  syncer::ModelType::OS_PRIORITY_PREFERENCES,
                  kOsPriorityPreferenceKey,
                  ConvertToSyncedPrefValue(kOsPriorityPreferenceNewValue))
                  .Wait());
  EXPECT_TRUE(FakeServerPrefMatchesValueChecker(
                  syncer::ModelType::PRIORITY_PREFERENCES,
                  kOsPriorityPreferenceKey,
                  ConvertToSyncedPrefValue(kOsPriorityPreferenceNewValue))
                  .Wait());
}

// Old clients should get synced prefs.
IN_PROC_BROWSER_TEST_F(SyncCategorizationDisabledTest, ReceiveSyncedOSPrefs) {
  InjectPreferenceToFakeServer(syncer::PREFERENCES, kOsPreferenceKey,
                               kOsPreferenceNewValue);
  InjectPreferenceToFakeServer(syncer::OS_PREFERENCES, kOsPreferenceKey,
                               kOsPreferenceNewValue);
  InjectPreferenceToFakeServer(syncer::PRIORITY_PREFERENCES,
                               kOsPriorityPreferenceKey,
                               kOsPriorityPreferenceNewValue);
  InjectPreferenceToFakeServer(syncer::OS_PRIORITY_PREFERENCES,
                               kOsPriorityPreferenceKey,
                               kOsPriorityPreferenceNewValue);

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  EXPECT_EQ(*GetPrefs(/*index=*/0)->Get(kOsPreferenceKey),
            kOsPreferenceNewValue);
  EXPECT_EQ(*GetPrefs(/*index=*/0)->Get(kOsPriorityPreferenceKey),
            kOsPriorityPreferenceNewValue);
}

// OS preferences are syncing only as browser prefs on the old clients.
IN_PROC_BROWSER_TEST_F(SyncCategorizationDisabledTest,
                       OSPreferencesSyncOnlyAsBrowserPrefs) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  ASSERT_NE(*GetPrefs(0)->Get(kOsPreferenceKey), kOsPreferenceNewValue);
  ASSERT_NE(*GetPrefs(0)->Get(kOsPriorityPreferenceKey),
            kOsPriorityPreferenceNewValue);

  ASSERT_TRUE(SetupSync());

  GetPrefs(/*index=*/0)->Set(kOsPreferenceKey, kOsPreferenceNewValue);
  GetPrefs(/*index=*/0)
      ->Set(kOsPriorityPreferenceKey, kOsPriorityPreferenceNewValue);

  EXPECT_TRUE(FakeServerPrefMatchesValueChecker(
                  syncer::ModelType::PREFERENCES, kOsPreferenceKey,
                  ConvertToSyncedPrefValue(kOsPreferenceNewValue))
                  .Wait());
  EXPECT_FALSE(
      preferences_helper::GetPreferenceInFakeServer(
          syncer::ModelType::OS_PREFERENCES, kOsPreferenceKey, GetFakeServer())
          .has_value());

  EXPECT_TRUE(FakeServerPrefMatchesValueChecker(
                  syncer::ModelType::PRIORITY_PREFERENCES,
                  kOsPriorityPreferenceKey,
                  ConvertToSyncedPrefValue(kOsPriorityPreferenceNewValue))
                  .Wait());
  EXPECT_FALSE(preferences_helper::GetPreferenceInFakeServer(
                   syncer::ModelType::OS_PRIORITY_PREFERENCES, kOsPreferenceKey,
                   GetFakeServer())
                   .has_value());
}

// OS preferences are not getting synced from the browser prefs on the new
// clients.
IN_PROC_BROWSER_TEST_F(SyncCategorizationEnabledTest,
                       DontReceiveSyncedOSPrefsFromOldClients) {
  InjectPreferenceToFakeServer(syncer::PREFERENCES, kOsPreferenceKey,
                               kOsPreferenceNewValue);
  InjectPreferenceToFakeServer(syncer::PRIORITY_PREFERENCES,
                               kOsPriorityPreferenceKey,
                               kOsPriorityPreferenceNewValue);

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  EXPECT_TRUE(GetPrefs(/*index=*/0)
                  ->FindPreference(kOsPreferenceKey)
                  ->IsDefaultValue());
  EXPECT_TRUE(GetPrefs(/*index=*/0)
                  ->FindPreference(kOsPriorityPreferenceKey)
                  ->IsDefaultValue());
}

}  // namespace
