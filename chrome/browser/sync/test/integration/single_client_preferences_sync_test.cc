// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
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
#include "components/sync/driver/sync_service_impl.h"
#include "components/sync/engine/cycle/entity_change_metric_recording.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/preference_specifics.pb.h"
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

class SingleClientPreferencesSyncTest : public SyncTest {
 public:
  SingleClientPreferencesSyncTest() : SyncTest(SINGLE_CLIENT) {}

  SingleClientPreferencesSyncTest(const SingleClientPreferencesSyncTest&) =
      delete;
  SingleClientPreferencesSyncTest& operator=(
      const SingleClientPreferencesSyncTest&) = delete;

  ~SingleClientPreferencesSyncTest() override = default;

  // If non-empty, |contents| will be written to the Preferences file of the
  // profile at |index| before that Profile object is created.
  void SetPreexistingPreferencesFileContents(int index,
                                             const std::string& contents) {
    preexisting_preferences_file_contents_[index] = contents;
  }

 protected:
  void BeforeSetupClient(int index,
                         const base::FilePath& profile_path) override {
    const std::string& contents = preexisting_preferences_file_contents_[index];
    if (contents.empty()) {
      return;
    }

    base::FilePath pref_path(profile_path.Append(chrome::kPreferencesFilename));
    ASSERT_TRUE(base::CreateDirectory(profile_path));
    ASSERT_NE(-1,
              base::WriteFile(pref_path, contents.c_str(), contents.size()));
  }

 private:
  // The contents to be written to a profile's Preferences file before the
  // Profile object is created. If empty, no preexisting file will be written.
  // The map key corresponds to the profile's index.
  std::map<int, std::string> preexisting_preferences_file_contents_;
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

IN_PROC_BROWSER_TEST_F(SingleClientPreferencesSyncTest,
                       PRE_ShouldRemoveBadDataWhenRegistering) {
  // Populate the data store with data of type boolean but register as string.
  SetPreexistingPreferencesFileContents(
      0, "{\"testing\":{\"my-test-preference\":true}}");
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  PrefRegistrySyncable* registry = GetRegistry(GetProfile(0));
  registry->RegisterStringPref("testing.my-test-preference", "default-value",
                               user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);

  registry->RegisterStringPref("testing.unrelated-preference", "",
                               user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);

  // Make sure that the preference is exposed with its default value.
  const PrefService::Preference* preference =
      GetProfile(0)->GetPrefs()->FindPreference("testing.my-test-preference");
  ASSERT_THAT(preference, NotNull());
  EXPECT_THAT(preference->GetType(), Eq(base::Value::Type::STRING));
  EXPECT_THAT(preference->GetValue()->GetString(), Eq("default-value"));
  // This might actually expose a bug: IsDefaultValue() is looking for the
  // the store with highest priority which has a value for the preference's
  // name. For this, no type checks are done, and hence this value is not
  // recognized as a default value. --> file a bug!
  EXPECT_TRUE(preference->IsDefaultValue());

  // The next test (without "PRE_") will verify that the bad data was actually
  // removed from the disk.
  // Set some unrelated pref, so the next test can sanity check that the prefs
  // file was opened correctly.
  GetProfile(0)->GetPrefs()->SetString("testing.unrelated-preference", "cool");
}

IN_PROC_BROWSER_TEST_F(SingleClientPreferencesSyncTest,
                       ShouldRemoveBadDataWhenRegistering) {
  // Note: Do *not* call SetupClients() here, so that the Profile's PrefService
  // doesn't get created. (Otherwise it might interfere with manually reading
  // the prefs file below.)
  base::FilePath user_data_dir;
  base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  base::FilePath prefs_path = user_data_dir.Append(GetProfileBaseName(0))
                                  .Append(chrome::kPreferencesFilename);

  // To verify the bad data has been removed, read the JSON file from disk
  // directly (without going through PrefService).
  auto pref_store = base::MakeRefCounted<JsonPrefStore>(prefs_path);
  base::ScopedAllowBlockingForTesting allow_blocking;
  PersistentPrefStore::PrefReadError read_result = pref_store->ReadPrefs();
  ASSERT_EQ(read_result, PersistentPrefStore::PREF_READ_ERROR_NONE)
      << " Failed reading the prefs file into the store, error code "
      << read_result;

  // Sanity check: An unrelated pref that was set in the previous test is still
  // here.
  const base::Value* unrelated_pref;
  ASSERT_TRUE(
      pref_store->GetValue("testing.unrelated-preference", &unrelated_pref));
  ASSERT_TRUE(unrelated_pref->is_string());
  ASSERT_EQ(unrelated_pref->GetString(), "cool");

  // Finally, the actual test expectation: The pref which had a value of the
  // wrong type previously has been cleared.
  const base::Value* result;
  EXPECT_FALSE(pref_store->GetValue("testing.my-test-preference", &result));
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
  ASSERT_TRUE(GetClient(0)->AwaitEngineInitialization());

  // After restart, the last sync cycle snapshot should be empty.
  // Once a sync request happened (e.g. by a poll), that snapshot is populated.
  // We use the following checker to simply wait for an non-empty snapshot.
  EXPECT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());

  EXPECT_EQ(0, histogram_tester.GetBucketCount(
                   "Sync.ModelTypeEntityChange3.PREFERENCE",
                   syncer::ModelTypeEntityChange::kRemoteInitialUpdate));
}

}  // namespace
