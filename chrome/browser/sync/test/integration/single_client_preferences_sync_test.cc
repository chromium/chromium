// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>

#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/sync/test/integration/preferences_helper.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/updated_progress_marker_checker.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/pref_service.h"
#include "components/sync/driver/profile_sync_service.h"
#include "testing/gmock/include/gmock/gmock.h"

using preferences_helper::BooleanPrefMatches;
using preferences_helper::BuildPrefStoreFromPrefsFile;
using preferences_helper::ChangeBooleanPref;
using preferences_helper::GetRegistry;
using user_prefs::PrefRegistrySyncable;
using testing::Eq;
using testing::NotNull;

namespace {

class SingleClientPreferencesSyncTest : public SyncTest {
 public:
  SingleClientPreferencesSyncTest() : SyncTest(SINGLE_CLIENT) {}

  ~SingleClientPreferencesSyncTest() override {}

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

  DISALLOW_COPY_AND_ASSIGN(SingleClientPreferencesSyncTest);
};

IN_PROC_BROWSER_TEST_F(SingleClientPreferencesSyncTest, Sanity) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(BooleanPrefMatches(prefs::kHomePageIsNewTabPage));
  ChangeBooleanPref(0, prefs::kHomePageIsNewTabPage);
  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());
  EXPECT_TRUE(BooleanPrefMatches(prefs::kHomePageIsNewTabPage));
}

// Flaky on Windows. https://crbug.com/930482
#if defined(OS_WIN)
#define MAYBE_ShouldRemoveBadDataWhenRegistering \
  DISABLED_ShouldRemoveBadDataWhenRegistering
#else
#define MAYBE_ShouldRemoveBadDataWhenRegistering \
  ShouldRemoveBadDataWhenRegistering
#endif
IN_PROC_BROWSER_TEST_F(SingleClientPreferencesSyncTest,
                       MAYBE_ShouldRemoveBadDataWhenRegistering) {
  // Populate the data store with data of type boolean but register as string.
  SetPreexistingPreferencesFileContents(
      0, "{\"testing\":{\"my-test-preference\":true}}");
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  PrefRegistrySyncable* registry = GetRegistry(GetProfile(0));
  registry->RegisterStringPref("testing.my-test-preference", "default-value",
                               user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
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

  // To verify the bad data has been removed, we read the JSON file from disk.
  scoped_refptr<PrefStore> pref_store =
      BuildPrefStoreFromPrefsFile(GetProfile(0));
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
  EXPECT_EQ(kNumEntities, histogram_tester.GetBucketCount(
                              "Sync.ModelTypeEntityChange3.PREFERENCE",
                              /*REMOTE_INITIAL_UPDATE=*/5));
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
                   /*REMOTE_INITIAL_UPDATE=*/5));
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
#if defined(CHROMEOS)
  // signin::SetRefreshTokenForPrimaryAccount() is needed on ChromeOS in order
  // to get a non-empty refresh token on startup.
  GetClient(0)->SignInPrimaryAccount();
#endif  // defined(CHROMEOS)
  ASSERT_TRUE(GetClient(0)->AwaitEngineInitialization());

  // After restart, the last sync cycle snapshot should be empty.
  // Once a sync request happened (e.g. by a poll), that snapshot is populated.
  // We use the following checker to simply wait for an non-empty snapshot.
  EXPECT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());

  EXPECT_EQ(0, histogram_tester.GetBucketCount(
                   "Sync.ModelTypeEntityChange3.PREFERENCE",
                   /*REMOTE_INITIAL_UPDATE=*/5));
}

}  // namespace
