// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/shelf_prefs.h"
#include "base/json/json_writer.h"
#include "base/notreached.h"
#include "base/values.h"
#include "chrome/browser/sync/test/integration/preferences_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/updated_progress_marker_checker.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/data_type.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using preferences_helper::ChangeStringPref;
using preferences_helper::GetPrefs;
using testing::Eq;

class SingleClientOsPreferencesSyncTest : public SyncTest {
 public:
  SingleClientOsPreferencesSyncTest() : SyncTest(SINGLE_CLIENT) {}
  ~SingleClientOsPreferencesSyncTest() override = default;

 protected:
  static std::string ConvertToSyncedPrefValue(const base::Value& value) {
    std::string result;
    bool success = base::JSONWriter::Write(value, &result);
    DCHECK(success);
    return result;
  }

  sync_pb::PreferenceSpecifics* GetPreferenceSpecifics(
      syncer::DataType data_type,
      sync_pb::EntitySpecifics& specifics) {
    switch (data_type) {
      case syncer::DataType::PREFERENCES:
        return specifics.mutable_preference();
      case syncer::DataType::PRIORITY_PREFERENCES:
        return specifics.mutable_priority_preference()->mutable_preference();
      case syncer::DataType::OS_PREFERENCES:
        return specifics.mutable_os_preference()->mutable_preference();
      case syncer::DataType::OS_PRIORITY_PREFERENCES:
        return specifics.mutable_os_priority_preference()->mutable_preference();
      default:
        NOTREACHED_IN_MIGRATION();
        return specifics.mutable_preference();
    }
  }
  void InjectPreferenceToFakeServer(syncer::DataType data_type,
                                    const char* name,
                                    const base::Value& value) {
    sync_pb::EntitySpecifics specifics;
    sync_pb::PreferenceSpecifics* preference_specifics =
        GetPreferenceSpecifics(data_type, specifics);
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
  const char* const kOsPriorityPreferenceKey = ash::prefs::kTapToClickEnabled;
  const base::Value kOsPriorityPreferenceNewValue = base::Value(false);
};

IN_PROC_BROWSER_TEST_F(SingleClientOsPreferencesSyncTest, Sanity) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // Shelf alignment is a Chrome OS only preference.
  ChangeStringPref(/*index=*/0, ash::prefs::kShelfAlignment,
                   ash::kShelfAlignmentRight);
  EXPECT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());
  EXPECT_THAT(GetPrefs(/*index=*/0)->GetString(ash::prefs::kShelfAlignment),
              Eq(ash::kShelfAlignmentRight));
}

IN_PROC_BROWSER_TEST_F(SingleClientOsPreferencesSyncTest,
                       OSPreferencesAreUploaded) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  ASSERT_NE(GetPrefs(0)->GetValue(kOsPreferenceKey), kOsPreferenceNewValue);
  ASSERT_NE(GetPrefs(0)->GetValue(kOsPriorityPreferenceKey),
            kOsPriorityPreferenceNewValue);

  ASSERT_TRUE(SetupSync());

  GetPrefs(/*index=*/0)->Set(kOsPreferenceKey, kOsPreferenceNewValue);
  GetPrefs(/*index=*/0)
      ->Set(kOsPriorityPreferenceKey, kOsPriorityPreferenceNewValue);

  EXPECT_TRUE(FakeServerPrefMatchesValueChecker(
                  syncer::DataType::OS_PREFERENCES, kOsPreferenceKey,
                  ConvertToSyncedPrefValue(kOsPreferenceNewValue))
                  .Wait());
  EXPECT_TRUE(FakeServerPrefMatchesValueChecker(
                  syncer::DataType::OS_PRIORITY_PREFERENCES,
                  kOsPriorityPreferenceKey,
                  ConvertToSyncedPrefValue(kOsPriorityPreferenceNewValue))
                  .Wait());
}

// OS preferences are not getting synced from the browser prefs on the new
// clients.
IN_PROC_BROWSER_TEST_F(SingleClientOsPreferencesSyncTest,
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
