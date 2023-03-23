// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/sync/test/integration/preferences_helper.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/updated_progress_marker_checker.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/features.h"
#include "components/sync/base/model_type.h"
#include "components/sync/driver/sync_service_impl.h"
#include "components/sync/engine/cycle/entity_change_metric_recording.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/preference_specifics.pb.h"
#include "components/sync_preferences/common_syncable_prefs_database.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {

using preferences_helper::ChangeBooleanPref;
using preferences_helper::GetPrefs;
using preferences_helper::GetRegistry;
using testing::Eq;
using testing::Ne;
using testing::NotNull;
using user_prefs::PrefRegistrySyncable;

std::string ConvertToSyncedPrefValue(const base::Value& value) {
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

class SingleClientPreferencesSyncTest : public SyncTest {
 public:
  SingleClientPreferencesSyncTest() : SyncTest(SINGLE_CLIENT) {}

  SingleClientPreferencesSyncTest(const SingleClientPreferencesSyncTest&) =
      delete;
  SingleClientPreferencesSyncTest& operator=(
      const SingleClientPreferencesSyncTest&) = delete;

  ~SingleClientPreferencesSyncTest() override = default;

 protected:
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
};

IN_PROC_BROWSER_TEST_F(SingleClientPreferencesSyncTest, Sanity) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  const bool kDefaultValue =
      GetPrefs(/*index=*/0)->GetBoolean(prefs::kHomePageIsNewTabPage);
  ChangeBooleanPref(0, prefs::kHomePageIsNewTabPage);
  EXPECT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());
  EXPECT_THAT(GetPrefs(/*index=*/0)->GetBoolean(prefs::kHomePageIsNewTabPage),
              Ne(kDefaultValue));
}

// Regression test to verify that pagination during GetUpdates() contributes
// properly to UMA histograms.
IN_PROC_BROWSER_TEST_F(SingleClientPreferencesSyncTest,
                       EmitModelTypeEntityChangeToUma) {
  const int kNumEntities = 17;

  fake_server_->SetMaxGetUpdatesBatchSize(7);

  sync_pb::EntitySpecifics specifics;
  for (int i = 0; i < kNumEntities; i++) {
    specifics.mutable_preference()->set_name(base::StringPrintf("pref%d", i));
    fake_server_->InjectEntity(
        syncer::PersistentUniqueClientEntity::CreateFromSpecificsForTesting(
            /*non_unique_name=*/"",
            /*client_tag=*/specifics.preference().name(), specifics,
            /*creation_time=*/0, /*last_modified_time=*/0));
  }

  base::HistogramTester histogram_tester;
  ASSERT_TRUE(SetupSync());
  EXPECT_EQ(kNumEntities,
            histogram_tester.GetBucketCount(
                "Sync.ModelTypeEntityChange3.PREFERENCE",
                syncer::ModelTypeEntityChange::kRemoteInitialUpdate));
}

IN_PROC_BROWSER_TEST_F(SingleClientPreferencesSyncTest,
                       PRE_PersistProgressMarkerOnRestart) {
  sync_pb::EntitySpecifics specifics;
  specifics.mutable_preference()->set_name("testing.my-test-preference");
  fake_server_->InjectEntity(
      syncer::PersistentUniqueClientEntity::CreateFromSpecificsForTesting(
          /*non_unique_name=*/"",
          /*client_tag=*/specifics.preference().name(), specifics,
          /*creation_time=*/0,
          /*last_modified_time=*/0));

  base::HistogramTester histogram_tester;
  ASSERT_TRUE(SetupSync());
  EXPECT_EQ(1, histogram_tester.GetBucketCount(
                   "Sync.ModelTypeEntityChange3.PREFERENCE",
                   syncer::ModelTypeEntityChange::kRemoteInitialUpdate));
}

IN_PROC_BROWSER_TEST_F(SingleClientPreferencesSyncTest,
                       PersistProgressMarkerOnRestart) {
  sync_pb::EntitySpecifics specifics;
  specifics.mutable_preference()->set_name("testing.my-test-preference");
  fake_server_->InjectEntity(
      syncer::PersistentUniqueClientEntity::CreateFromSpecificsForTesting(
          /*non_unique_name=*/"",
          /*client_tag=*/specifics.preference().name(), specifics,
          /*creation_time=*/0,
          /*last_modified_time=*/0));

  base::HistogramTester histogram_tester;
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  ASSERT_TRUE(GetClient(0)->AwaitSyncSetupCompletion());

  // After restart, the last sync cycle snapshot should be empty.
  // Once a sync request happened (e.g. by a poll), that snapshot is populated.
  // We use the following checker to simply wait for an non-empty snapshot.
  GetSyncService(0)->TriggerRefresh({syncer::PREFERENCES});
  EXPECT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());

  EXPECT_EQ(0, histogram_tester.GetBucketCount(
                   "Sync.ModelTypeEntityChange3.PREFERENCE",
                   syncer::ModelTypeEntityChange::kRemoteInitialUpdate));
}

// Verifies that priority synced preferences and regular synced preferences are
// kept separate. Tests that incoming priority preference change does not have
// any effect if the corresponding pref is registered as a regular preference.
IN_PROC_BROWSER_TEST_F(SingleClientPreferencesSyncTest,
                       ShouldIsolatePreferencesOfDifferentTypes) {
  // Register a pref as regular synced with client.
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  GetRegistry(GetProfile(0))
      ->RegisterStringPref(sync_preferences::kSyncablePrefForTesting, "",
                           user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  preferences_helper::ChangeStringPref(
      0, sync_preferences::kSyncablePrefForTesting, "non-priority value");

  // Create similar entity on the server but as a priority preference.
  InjectPreferenceToFakeServer(syncer::PRIORITY_PREFERENCES,
                               sync_preferences::kSyncablePrefForTesting,
                               base::Value("priority value"));

  ASSERT_TRUE(SetupSync());

  // Value remains unchanged.
  EXPECT_THAT(GetPrefs(0)->GetString(sync_preferences::kSyncablePrefForTesting),
              Eq("non-priority value"));
}

class SingleClientPreferencesWithAccountStorageSyncTest
    : public SingleClientPreferencesSyncTest {
 public:
  SingleClientPreferencesWithAccountStorageSyncTest()
      : feature_list_(syncer::kEnablePreferencesAccountStorage) {}

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(SingleClientPreferencesWithAccountStorageSyncTest,
                       ShouldPreserveLocalPrefsAndNotUploadToAccountOnSignin) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  // Register `sync_preferences::kSyncablePrefForTesting`.
  GetRegistry(GetProfile(0))
      ->RegisterStringPref(sync_preferences::kSyncablePrefForTesting, "",
                           user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  ASSERT_FALSE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::PREFERENCES));

  preferences_helper::ChangeStringPref(
      0, sync_preferences::kSyncablePrefForTesting, "local value");

  // Enable Sync.
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::PREFERENCES));

  // Local value is preserved as the pref doesn't exist on the account.
  EXPECT_EQ(GetPrefs(0)->GetString(sync_preferences::kSyncablePrefForTesting),
            "local value");
  // No data is uploaded to the account.
  EXPECT_FALSE(preferences_helper::GetPreferenceInFakeServer(
                   syncer::PREFERENCES,
                   sync_preferences::kSyncablePrefForTesting, GetFakeServer())
                   .has_value());
}

// ChromeOS does not support signing out of a primary account.
#if !BUILDFLAG(IS_CHROMEOS)

IN_PROC_BROWSER_TEST_F(SingleClientPreferencesWithAccountStorageSyncTest,
                       ShouldCleanupAccountStoreOnSignout) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  // Register `sync_preferences::kSyncablePrefForTesting`.
  GetRegistry(GetProfile(0))
      ->RegisterStringPref(sync_preferences::kSyncablePrefForTesting, "",
                           user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  ASSERT_FALSE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::PREFERENCES));

  preferences_helper::ChangeStringPref(
      0, sync_preferences::kSyncablePrefForTesting, "local value");

  InjectPreferenceToFakeServer(syncer::PREFERENCES,
                               sync_preferences::kSyncablePrefForTesting,
                               base::Value("account value"));

  // Sign in and enable Sync.
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::PREFERENCES));

  // Fake server value is synced to the account store and overrides local value.
  EXPECT_EQ(GetPrefs(0)->GetString(sync_preferences::kSyncablePrefForTesting),
            "account value");

  // Sign out.
  GetClient(0)->SignOutPrimaryAccount();
  ASSERT_FALSE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::PREFERENCES));

  // Account value gets cleared. Local value persists.
  EXPECT_EQ(GetPrefs(0)->GetString(sync_preferences::kSyncablePrefForTesting),
            "local value");
}

#endif  // !BUILDFLAG(IS_CHROMEOS)

IN_PROC_BROWSER_TEST_F(SingleClientPreferencesWithAccountStorageSyncTest,
                       ShouldCleanupAccountStoreOnDisable) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  // Register `sync_preferences::kSyncablePrefForTesting`.
  GetRegistry(GetProfile(0))
      ->RegisterStringPref(sync_preferences::kSyncablePrefForTesting, "",
                           user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  ASSERT_FALSE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::PREFERENCES));

  preferences_helper::ChangeStringPref(
      0, sync_preferences::kSyncablePrefForTesting, "local value");

  InjectPreferenceToFakeServer(syncer::PREFERENCES,
                               sync_preferences::kSyncablePrefForTesting,
                               base::Value("account value"));

  // Enable Sync.
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::PREFERENCES));

  // Fake server value is synced to the account store and overrides local value.
  EXPECT_EQ(GetPrefs(0)->GetString(sync_preferences::kSyncablePrefForTesting),
            "account value");

  // Disable syncing preferences.
  ASSERT_TRUE(GetClient(0)->DisableSyncForType(
      syncer::UserSelectableType::kPreferences));
  ASSERT_FALSE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::PREFERENCES));

  // Account value gets cleared. Local value persists.
  EXPECT_EQ(GetPrefs(0)->GetString(sync_preferences::kSyncablePrefForTesting),
            "local value");
}

IN_PROC_BROWSER_TEST_F(SingleClientPreferencesWithAccountStorageSyncTest,
                       ShouldChangeSyncablePrefLocallyAndOnAccount) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  // Register `sync_preferences::kSyncablePrefForTesting`.
  GetRegistry(GetProfile(0))
      ->RegisterStringPref(sync_preferences::kSyncablePrefForTesting, "",
                           user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  ASSERT_FALSE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::PREFERENCES));

  preferences_helper::ChangeStringPref(
      0, sync_preferences::kSyncablePrefForTesting, "local value");

  InjectPreferenceToFakeServer(syncer::PREFERENCES,
                               sync_preferences::kSyncablePrefForTesting,
                               base::Value("account value"));

  // Enable Sync.
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::PREFERENCES));

  // Fake server value is synced to the account store and overrides local value.
  ASSERT_EQ(GetPrefs(0)->GetString(sync_preferences::kSyncablePrefForTesting),
            "account value");
  // Change pref value.
  preferences_helper::ChangeStringPref(
      0, sync_preferences::kSyncablePrefForTesting, "new value");

  // Change is synced to account.
  EXPECT_TRUE(FakeServerPrefMatchesValueChecker(
                  syncer::ModelType::PREFERENCES,
                  sync_preferences::kSyncablePrefForTesting,
                  ConvertToSyncedPrefValue(base::Value("new value")))
                  .Wait());

  // Disable syncing preferences.
  ASSERT_TRUE(GetClient(0)->DisableSyncForType(
      syncer::UserSelectableType::kPreferences));
  ASSERT_FALSE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::PREFERENCES));

  // Change was also made to local store.
  EXPECT_EQ(GetPrefs(0)->GetString(sync_preferences::kSyncablePrefForTesting),
            "new value");
}

IN_PROC_BROWSER_TEST_F(SingleClientPreferencesWithAccountStorageSyncTest,
                       ShouldNotSyncNonSyncablePrefToAccount) {
  constexpr char kNonSyncablePref[] = "non-syncable pref";

  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  // Register prefs.
  GetRegistry(GetProfile(0))->RegisterStringPref(kNonSyncablePref, "", 0);
  GetRegistry(GetProfile(0))
      ->RegisterStringPref(sync_preferences::kSyncablePrefForTesting, "",
                           user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  ASSERT_FALSE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::PREFERENCES));

  preferences_helper::ChangeStringPref(0, kNonSyncablePref, "local value");
  InjectPreferenceToFakeServer(syncer::PREFERENCES, kNonSyncablePref,
                               base::Value("account value"));

  // Enable Sync.
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::PREFERENCES));

  // Update prefs.
  preferences_helper::ChangeStringPref(0, kNonSyncablePref, "new local value");
  preferences_helper::ChangeStringPref(
      0, sync_preferences::kSyncablePrefForTesting, "new value");

  // Change is synced to account.
  EXPECT_TRUE(FakeServerPrefMatchesValueChecker(
                  syncer::ModelType::PREFERENCES,
                  sync_preferences::kSyncablePrefForTesting,
                  ConvertToSyncedPrefValue(base::Value("new value")))
                  .Wait());
  // Not the right way to test this but the non-syncable pref has not been
  // synced to the new value.
  EXPECT_TRUE(FakeServerPrefMatchesValueChecker(
                  syncer::ModelType::PREFERENCES, kNonSyncablePref,
                  ConvertToSyncedPrefValue(base::Value("account value")))
                  .Wait());
}

}  // namespace
