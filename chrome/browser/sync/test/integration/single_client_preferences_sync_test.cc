// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>

#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/prefs/chrome_pref_service_factory.h"
#include "chrome/browser/subscription_eligibility/subscription_eligibility_prefs.h"
#include "chrome/browser/subscription_eligibility/subscription_eligibility_service.h"
#include "chrome/browser/subscription_eligibility/subscription_eligibility_service_factory.h"
#include "chrome/browser/sync/test/integration/preferences_helper.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/updated_progress_marker_checker.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/mock_pref_change_callback.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/features.h"
#include "components/sync/engine/cycle/entity_change_metric_recording.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/preference_specifics.pb.h"
#include "components/sync/service/sync_service_impl.h"
#include "components/sync_preferences/common_syncable_prefs_database.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_launcher.h"
#include "testing/gmock/include/gmock/gmock.h"

#if BUILDFLAG(ENABLE_GLIC)
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/test_support/glic_test_environment.h"
#include "chrome/browser/glic/test_support/glic_test_util.h"
#endif

namespace {

using preferences_helper::ChangeBooleanPref;
using preferences_helper::ConvertPrefValueToValueInSpecifics;
using preferences_helper::GetPrefs;
using preferences_helper::GetRegistry;
using testing::Eq;
using testing::Ne;
using testing::NotNull;
using user_prefs::PrefRegistrySyncable;

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
      NOTREACHED();
  }
}

// Reads a json file and returns it as a dict value. If `key` is provided, only
// the value for that key is returned. This returns nullopt if there was an
// error reading the values from the file, for example, the file doesn't exist.
// NOTE: `key` missing from the json file would be returned as an empty dict,
// and not a nullopt.
std::optional<base::Value::Dict> ReadValuesFromFile(
    const base::FilePath& file_path,
    const std::optional<std::string>& key = std::nullopt) {
  std::optional<base::Value::Dict> result;
  // ASSERT_* returns void. Thus using a lambda to not exit from the function,
  // but still generate fatal failures.
  [&]() {
    base::ScopedAllowBlockingForTesting allow_blocking;

    ASSERT_TRUE(base::PathExists(file_path))
        << "Preference file " << file_path << " does not exist.";
    std::string file_content;
    ASSERT_TRUE(base::ReadFileToString(file_path, &file_content));
    std::optional<base::Value> json_content = base::JSONReader::Read(
        file_content, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
    ASSERT_TRUE(json_content.has_value() && json_content->is_dict())
        << "Failed to parse file content: " << file_content;
    if (!key.has_value()) {
      result = std::move(json_content->GetDict());
    } else {
      base::Value::Dict* dict =
          json_content->GetDict().FindDictByDottedPath(key.value());
      result = dict ? std::move(*dict) : base::Value::Dict();
    }
  }();
  return result;
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
  void InjectPreferenceToFakeServer(syncer::DataType data_type,
                                    const char* name,
                                    const base::Value& value) {
    sync_pb::EntitySpecifics specifics;
    sync_pb::PreferenceSpecifics* preference_specifics =
        GetPreferenceSpecifics(data_type, specifics);
    preference_specifics->set_name(name);
    preference_specifics->set_value(ConvertPrefValueToValueInSpecifics(value));

    GetFakeServer()->InjectEntity(
        syncer::PersistentUniqueClientEntity::CreateFromSpecificsForTesting(
            /*non_unique_name=*/name,
            /*client_tag=*/name, specifics,
            /*creation_time=*/0, /*last_modified_time=*/0));
  }
};

IN_PROC_BROWSER_TEST_F(SingleClientPreferencesSyncTest, Sanity) {
  ASSERT_TRUE(SetupSync());

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
                       EmitDataTypeEntityChangeToUma) {
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
                "Sync.DataTypeEntityChange.PREFERENCE",
                syncer::DataTypeEntityChange::kRemoteInitialUpdate));
}

// TODO(crbug.com/40200835): PRE_ tests are not supported on Android.
#if !BUILDFLAG(IS_ANDROID)
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
                   "Sync.DataTypeEntityChange.PREFERENCE",
                   syncer::DataTypeEntityChange::kRemoteInitialUpdate));
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
  ASSERT_TRUE(SetupClients());
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());

  // After restart, the last sync cycle snapshot should be empty.
  // Once a sync request happened (e.g. by a poll), that snapshot is populated.
  // We use the following checker to simply wait for an non-empty snapshot.
  GetSyncService(0)->TriggerRefresh(
      syncer::SyncService::TriggerRefreshSource::kUnknown,
      {syncer::PREFERENCES});
  EXPECT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());

  EXPECT_EQ(0, histogram_tester.GetBucketCount(
                   "Sync.DataTypeEntityChange.PREFERENCE",
                   syncer::DataTypeEntityChange::kRemoteInitialUpdate));
}
#endif  // !BUILDFLAG(IS_ANDROID)

// Verifies that priority synced preferences and regular synced preferences are
// kept separate. Tests that incoming priority preference change does not have
// any effect if the corresponding pref is registered as a regular preference.
IN_PROC_BROWSER_TEST_F(SingleClientPreferencesSyncTest,
                       ShouldIsolatePreferencesOfDifferentTypes) {
  // Register a pref as regular synced with client.
  ASSERT_TRUE(SetupClients());
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
  SingleClientPreferencesWithAccountStorageSyncTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{switches::kEnablePreferencesAccountStorage,
                              // This is needed to enable prefs in transport
                              // mode.
                              syncer::kSeparateLocalAndAccountSearchEngines},
        /*disabled_features=*/{});
  }

  void CommitToDiskAndWait() const {
    base::RunLoop loop;
    GetPrefs(0)->CommitPendingWrite(loop.QuitClosure());
    loop.Run();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(SingleClientPreferencesWithAccountStorageSyncTest,
                       ShouldPreserveLocalPrefsAndNotUploadToAccountOnSignin) {
  ASSERT_TRUE(SetupClients());
  // Register `sync_preferences::kSyncablePrefForTesting`.
  GetRegistry(GetProfile(0))
      ->RegisterStringPref(sync_preferences::kSyncablePrefForTesting, "",
                           user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  ASSERT_FALSE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::PREFERENCES));

  preferences_helper::ChangeStringPref(
      0, sync_preferences::kSyncablePrefForTesting, "local value");

  // Enable Sync.
  ASSERT_TRUE(SetupSync());
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
  ASSERT_TRUE(SetupClients());
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
  ASSERT_TRUE(SetupSync());
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
  ASSERT_TRUE(SetupClients());
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
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::PREFERENCES));

  // Fake server value is synced to the account store and overrides local value.
  EXPECT_EQ(GetPrefs(0)->GetString(sync_preferences::kSyncablePrefForTesting),
            "account value");

  // Disable syncing preferences.
  ASSERT_TRUE(GetClient(0)->DisableSelectableType(
      syncer::UserSelectableType::kPreferences));
  ASSERT_FALSE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::PREFERENCES));

  // Account value gets cleared. Local value persists.
  EXPECT_EQ(GetPrefs(0)->GetString(sync_preferences::kSyncablePrefForTesting),
            "local value");
}

IN_PROC_BROWSER_TEST_F(SingleClientPreferencesWithAccountStorageSyncTest,
                       ShouldChangeSyncablePrefLocallyAndOnAccount) {
  ASSERT_TRUE(SetupClients());
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
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::PREFERENCES));

  // Fake server value is synced to the account store and overrides local value.
  ASSERT_EQ(GetPrefs(0)->GetString(sync_preferences::kSyncablePrefForTesting),
            "account value");
  // Change pref value.
  preferences_helper::ChangeStringPref(
      0, sync_preferences::kSyncablePrefForTesting, "new value");

  // Change is synced to account.
  EXPECT_TRUE(FakeServerPrefMatchesValueChecker(
                  syncer::DataType::PREFERENCES,
                  sync_preferences::kSyncablePrefForTesting,
                  ConvertPrefValueToValueInSpecifics(base::Value("new value")))
                  .Wait());

  // Disable syncing preferences.
  ASSERT_TRUE(GetClient(0)->DisableSelectableType(
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

  ASSERT_TRUE(SetupClients());
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
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::PREFERENCES));

  // Update prefs.
  preferences_helper::ChangeStringPref(0, kNonSyncablePref, "new local value");
  preferences_helper::ChangeStringPref(
      0, sync_preferences::kSyncablePrefForTesting, "new value");

  // Change is synced to account.
  EXPECT_TRUE(FakeServerPrefMatchesValueChecker(
                  syncer::DataType::PREFERENCES,
                  sync_preferences::kSyncablePrefForTesting,
                  ConvertPrefValueToValueInSpecifics(base::Value("new value")))
                  .Wait());
  // Not the right way to test this but the non-syncable pref has not been
  // synced to the new value.
  EXPECT_TRUE(
      FakeServerPrefMatchesValueChecker(
          syncer::DataType::PREFERENCES, kNonSyncablePref,
          ConvertPrefValueToValueInSpecifics(base::Value("account value")))
          .Wait());
}

IN_PROC_BROWSER_TEST_F(SingleClientPreferencesWithAccountStorageSyncTest,
                       ShouldNotSyncSensitivePrefsIfHistorySyncOff) {
  ASSERT_TRUE(SetupClients());
  // Register `sync_preferences::kSyncableHistorySensitiveListPrefForTesting`.
  GetRegistry(GetProfile(0))
      ->RegisterListPref(
          sync_preferences::kSyncableHistorySensitiveListPrefForTesting,
          user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  ASSERT_FALSE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::PREFERENCES));
  ASSERT_FALSE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::HISTORY));

  base::Value::List local_value;
  local_value.Append("local value");
  preferences_helper::ChangeListPref(
      0, sync_preferences::kSyncableHistorySensitiveListPrefForTesting,
      local_value);

  base::Value::List account_value;
  account_value.Append("account value");
  InjectPreferenceToFakeServer(
      syncer::PREFERENCES,
      sync_preferences::kSyncableHistorySensitiveListPrefForTesting,
      base::Value(account_value.Clone()));

  ASSERT_EQ(GetPrefs(0)->GetList(
                sync_preferences::kSyncableHistorySensitiveListPrefForTesting),
            local_value);

  // Enable Sync but not history data type.
  ASSERT_TRUE(GetClient(0)->SetupSyncWithCustomSettings(
      base::BindOnce([](syncer::SyncUserSettings* settings) {
        syncer::UserSelectableTypeSet types =
            settings->GetRegisteredSelectableTypes();
        types.Remove(syncer::UserSelectableType::kHistory);
        settings->SetSelectedTypes(/*sync_everything=*/false, types);
#if !BUILDFLAG(IS_CHROMEOS)
        settings->SetInitialSyncFeatureSetupComplete(
            syncer::SyncFirstSetupCompleteSource::ADVANCED_FLOW_CONFIRM);
#endif  // !BUILDFLAG(IS_CHROMEOS)
      })));

  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::PREFERENCES));
  ASSERT_FALSE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::HISTORY));

  // Account value is not returned.
  EXPECT_EQ(GetPrefs(0)->GetList(
                sync_preferences::kSyncableHistorySensitiveListPrefForTesting),
            local_value);

  base::Value::List new_value;
  new_value.Append("new value");
  preferences_helper::ChangeListPref(
      0, sync_preferences::kSyncableHistorySensitiveListPrefForTesting,
      new_value);

  ASSERT_TRUE(AwaitQuiescence());
  // New value is not uploaded to the account.
  EXPECT_NE(preferences_helper::GetPreferenceInFakeServer(
                syncer::PREFERENCES,
                sync_preferences::kSyncableHistorySensitiveListPrefForTesting,
                GetFakeServer())
                ->value(),
            ConvertPrefValueToValueInSpecifics(base::Value(new_value.Clone())));
}

IN_PROC_BROWSER_TEST_F(SingleClientPreferencesWithAccountStorageSyncTest,
                       ShouldSyncSensitivePrefsIfHistorySyncOn) {
  ASSERT_TRUE(SetupClients());
  // Register `sync_preferences::kSyncableHistorySensitiveListPrefForTesting`.
  GetRegistry(GetProfile(0))
      ->RegisterListPref(
          sync_preferences::kSyncableHistorySensitiveListPrefForTesting,
          user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  ASSERT_FALSE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::PREFERENCES));
  ASSERT_FALSE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::HISTORY));

  base::Value::List local_value;
  local_value.Append("local value");
  preferences_helper::ChangeListPref(
      0, sync_preferences::kSyncableHistorySensitiveListPrefForTesting,
      local_value);

  base::Value::List account_value;
  account_value.Append("account value");
  InjectPreferenceToFakeServer(
      syncer::PREFERENCES,
      sync_preferences::kSyncableHistorySensitiveListPrefForTesting,
      base::Value(account_value.Clone()));

  // Enable Sync.
  ASSERT_TRUE(SetupSync());

  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::PREFERENCES));
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::HISTORY));

  // Account value is returned.
  EXPECT_EQ(GetPrefs(0)->GetList(
                sync_preferences::kSyncableHistorySensitiveListPrefForTesting),
            account_value);

  base::Value::List new_value;
  new_value.Append("new value");
  preferences_helper::ChangeListPref(
      0, sync_preferences::kSyncableHistorySensitiveListPrefForTesting,
      new_value);

  // New value is synced to account.
  EXPECT_TRUE(
      FakeServerPrefMatchesValueChecker(
          syncer::DataType::PREFERENCES,
          sync_preferences::kSyncableHistorySensitiveListPrefForTesting,
          ConvertPrefValueToValueInSpecifics(base::Value(new_value.Clone())))
          .Wait());
}

IN_PROC_BROWSER_TEST_F(SingleClientPreferencesWithAccountStorageSyncTest,
                       ShouldListenToHistorySyncOptInChanges) {
  ASSERT_TRUE(SetupClients());
  // Register `sync_preferences::kSyncableHistorySensitiveListPrefForTesting`.
  GetRegistry(GetProfile(0))
      ->RegisterListPref(
          sync_preferences::kSyncableHistorySensitiveListPrefForTesting,
          user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  ASSERT_FALSE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::PREFERENCES));
  ASSERT_FALSE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::HISTORY));

  base::Value::List local_value;
  local_value.Append("local value");
  preferences_helper::ChangeListPref(
      0, sync_preferences::kSyncableHistorySensitiveListPrefForTesting,
      local_value);

  base::Value::List account_value;
  account_value.Append("account value");
  InjectPreferenceToFakeServer(
      syncer::PREFERENCES,
      sync_preferences::kSyncableHistorySensitiveListPrefForTesting,
      base::Value(account_value.Clone()));

  // Enable Sync but not history data type.
  ASSERT_TRUE(GetClient(0)->SetupSyncWithCustomSettings(
      base::BindOnce([](syncer::SyncUserSettings* settings) {
        syncer::UserSelectableTypeSet types =
            settings->GetRegisteredSelectableTypes();
        types.Remove(syncer::UserSelectableType::kHistory);
        settings->SetSelectedTypes(/*sync_everything=*/false, types);
#if !BUILDFLAG(IS_CHROMEOS)
        settings->SetInitialSyncFeatureSetupComplete(
            syncer::SyncFirstSetupCompleteSource::ADVANCED_FLOW_CONFIRM);
#endif  // !BUILDFLAG(IS_CHROMEOS)
      })));

  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::PREFERENCES));
  ASSERT_FALSE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::HISTORY));

  // Account value is not returned.
  ASSERT_EQ(GetPrefs(0)->GetList(
                sync_preferences::kSyncableHistorySensitiveListPrefForTesting),
            local_value);

  // Enable history sync.
  ASSERT_TRUE(
      GetClient(0)->EnableSelectableType(syncer::UserSelectableType::kHistory));

  // Account value is now returned.
  ASSERT_EQ(GetPrefs(0)->GetList(
                sync_preferences::kSyncableHistorySensitiveListPrefForTesting),
            account_value);

  // Disable history sync.
  ASSERT_TRUE(GetClient(0)->DisableSelectableType(
      syncer::UserSelectableType::kHistory));

  // Account value is not returned.
  ASSERT_EQ(GetPrefs(0)->GetList(
                sync_preferences::kSyncableHistorySensitiveListPrefForTesting),
            local_value);
}

// Regression test for crbug.com/1456872.
// ChromeOS does not support signing out of a primary account.
#if !BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(SingleClientPreferencesWithAccountStorageSyncTest,
                       ShouldHandleWalletSideEffectsWhenSyncDisabled) {
  ASSERT_TRUE(SetupClients());
  ASSERT_FALSE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::PREFERENCES));

  InjectPreferenceToFakeServer(syncer::PREFERENCES,
                               autofill::prefs::kAutofillCreditCardEnabled,
                               base::Value(false));

  // Enable Sync.
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::PREFERENCES));

  // Fake server value is synced to the account store and overrides local value.
  ASSERT_FALSE(
      GetPrefs(0)->GetBoolean(autofill::prefs::kAutofillCreditCardEnabled));

  // kAutofillCreditCardEnabled prevents AUTOFILL_WALLET from running.
  ASSERT_FALSE(GetSyncService(0)->GetActiveDataTypes().Has(
      syncer::AUTOFILL_WALLET_DATA));

  // Disable sync, the data and metadata should be gone, without crashes.
  GetClient(0)->SignOutPrimaryAccount();

  ASSERT_FALSE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::PREFERENCES));

  // Enabling sync again should work, without crashes.
  EXPECT_TRUE(SetupSync());
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

#if !BUILDFLAG(IS_ANDROID)

IN_PROC_BROWSER_TEST_F(SingleClientPreferencesWithAccountStorageSyncTest,
                       ShouldCleanupAccountPreferencesFileOnDisable) {
  ASSERT_TRUE(SetupClients());
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
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::PREFERENCES));
  // Fake server value is synced to the account store and overrides local value.
  ASSERT_EQ(GetPrefs(0)->GetString(sync_preferences::kSyncablePrefForTesting),
            "account value");

  CommitToDiskAndWait();

  // Verify file content, `kSyncablePrefForTesting` is present.
  std::optional<base::Value::Dict> file_content = ReadValuesFromFile(
      GetProfile(0)->GetPath().Append(chrome::kPreferencesFilename),
      chrome_prefs::kAccountPreferencesPrefix);
  ASSERT_TRUE(file_content.has_value());

  std::string* value =
      file_content->FindString(sync_preferences::kSyncablePrefForTesting);
  ASSERT_TRUE(value);
  EXPECT_EQ(*value, "account value");

  // Disable syncing preferences. This should lead to clearing of account prefs
  // file.
  ASSERT_TRUE(GetClient(0)->DisableSelectableType(
      syncer::UserSelectableType::kPreferences));
  ASSERT_FALSE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::PREFERENCES));
  ASSERT_EQ(GetPrefs(0)->GetString(sync_preferences::kSyncablePrefForTesting),
            "local value");

  CommitToDiskAndWait();

  // Account prefs have been removed from the file (but prefs with
  // kExemptFromUserControlWhileSignedIn may still be there).
  file_content = ReadValuesFromFile(
      GetProfile(0)->GetPath().Append(chrome::kPreferencesFilename),
      chrome_prefs::kAccountPreferencesPrefix);
  ASSERT_TRUE(file_content.has_value());
  EXPECT_FALSE(
      file_content->FindString(sync_preferences::kSyncablePrefForTesting));
}

#if !BUILDFLAG(IS_CHROMEOS)

IN_PROC_BROWSER_TEST_F(SingleClientPreferencesWithAccountStorageSyncTest,
                       ShouldCleanupAccountPreferencesFileOnSignout) {
  ASSERT_TRUE(SetupClients());
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
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::PREFERENCES));
  // Fake server value is synced to the account store and overrides local value.
  ASSERT_EQ(GetPrefs(0)->GetString(sync_preferences::kSyncablePrefForTesting),
            "account value");

  CommitToDiskAndWait();

  // Verify file content, `kSyncablePrefForTesting` is present.
  std::optional<base::Value::Dict> file_content = ReadValuesFromFile(
      GetProfile(0)->GetPath().Append(chrome::kPreferencesFilename),
      chrome_prefs::kAccountPreferencesPrefix);
  ASSERT_TRUE(file_content.has_value());

  std::string* value =
      file_content->FindString(sync_preferences::kSyncablePrefForTesting);
  ASSERT_TRUE(value);
  EXPECT_EQ(*value, "account value");

  // Signout. This should lead to clearing of account prefs file.
  GetClient(0)->SignOutPrimaryAccount();
  ASSERT_FALSE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::PREFERENCES));
  ASSERT_EQ(GetPrefs(0)->GetString(sync_preferences::kSyncablePrefForTesting),
            "local value");

  CommitToDiskAndWait();

  // Account prefs have been removed from the file.
  file_content = ReadValuesFromFile(
      GetProfile(0)->GetPath().Append(chrome::kPreferencesFilename),
      chrome_prefs::kAccountPreferencesPrefix);
  ASSERT_TRUE(file_content.has_value());
  EXPECT_TRUE(file_content->empty());
}

#endif  // !BUILDFLAG(IS_CHROMEOS)
#endif  // !BUILDFLAG(IS_ANDROID)

// TODO(crbug.com/40200835): PRE_ tests are not supported on Android.
#if !BUILDFLAG(IS_ANDROID)
// Adds pref values to persistent storage.
IN_PROC_BROWSER_TEST_F(
    SingleClientPreferencesWithAccountStorageSyncTest,
    PRE_ShouldReadAccountPreferencesFromFileBeforeSyncStart) {
  ASSERT_TRUE(SetupClients());
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
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::PREFERENCES));
  // Fake server value is synced to the account store and overrides local value.
  ASSERT_EQ(GetPrefs(0)->GetString(sync_preferences::kSyncablePrefForTesting),
            "account value");
}

IN_PROC_BROWSER_TEST_F(SingleClientPreferencesWithAccountStorageSyncTest,
                       ShouldReadAccountPreferencesFromFileBeforeSyncStart) {
  ASSERT_TRUE(SetupClients());
  // Register `sync_preferences::kSyncablePrefForTesting`.
  GetRegistry(GetProfile(0))
      ->RegisterStringPref(sync_preferences::kSyncablePrefForTesting, "",
                           user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);

  // Sync has not started up yet, and thus PREFERENCES is not active yet.
  ASSERT_FALSE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::PREFERENCES));
  // However, the account value should still apply.
  EXPECT_EQ(GetPrefs(0)->GetString(sync_preferences::kSyncablePrefForTesting),
            "account value");
}

// Adds pref values to persistent storage.
IN_PROC_BROWSER_TEST_F(SingleClientPreferencesWithAccountStorageSyncTest,
                       PRE_ShouldNotNotifyUponSyncStart) {
  ASSERT_TRUE(SetupClients());
  // Register `sync_preferences::kSyncablePrefForTesting`.
  GetRegistry(GetProfile(0))
      ->RegisterBooleanPref(sync_preferences::kSyncablePrefForTesting, false,
                            user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);

  InjectPreferenceToFakeServer(syncer::PREFERENCES,
                               sync_preferences::kSyncablePrefForTesting,
                               base::Value(true));

  // Enable Sync.
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::PREFERENCES));
  // Fake server value is synced to the account store.
  ASSERT_TRUE(
      GetPrefs(0)->GetBoolean(sync_preferences::kSyncablePrefForTesting));
}

// Regression test for crbug.com/1470161.
IN_PROC_BROWSER_TEST_F(SingleClientPreferencesWithAccountStorageSyncTest,
                       ShouldNotNotifyUponSyncStart) {
  ASSERT_TRUE(SetupClients());
  // Register `sync_preferences::kSyncablePrefForTesting`.
  GetRegistry(GetProfile(0))
      ->RegisterBooleanPref(sync_preferences::kSyncablePrefForTesting, false,
                            user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);

  MockPrefChangeCallback observer(GetPrefs(0));
  PrefChangeRegistrar registrar;
  registrar.Init(GetPrefs(0));
  registrar.Add(sync_preferences::kSyncablePrefForTesting,
                observer.GetCallback());

  // Pref value is restored from the persisted json layer and never changes.
  EXPECT_CALL(observer, OnPreferenceChanged).Times(0);

  // Sync has not started up yet, and thus PREFERENCES is not active yet.
  ASSERT_FALSE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::PREFERENCES));

  // The account value is loaded from the persistence layer.
  ASSERT_TRUE(
      GetPrefs(0)->GetBoolean(sync_preferences::kSyncablePrefForTesting));

  // Explicitly set the pref value before sync is initialized. Since the
  // effective value doesn't change, this should be a no-op.
  GetPrefs(0)->SetBoolean(sync_preferences::kSyncablePrefForTesting, true);

  // Wait for sync to start. This would read sync data but should not result in
  // any changes to effective pref value, and thus not cause any observer calls.
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::PREFERENCES));
  ASSERT_TRUE(
      GetPrefs(0)->GetBoolean(sync_preferences::kSyncablePrefForTesting));
}

#endif  // !BUILDFLAG(IS_ANDROID)

using SingleClientPreferencesWithAccountStorageMergeSyncTest =
    SingleClientPreferencesWithAccountStorageSyncTest;

IN_PROC_BROWSER_TEST_F(SingleClientPreferencesWithAccountStorageMergeSyncTest,
                       ShouldMergeLocalAndAccountMergeableDictPref) {
  ASSERT_TRUE(SetupClients());
  ASSERT_FALSE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::PREFERENCES));

  GetRegistry(GetProfile(0))
      ->RegisterDictionaryPref(
          sync_preferences::kSyncableMergeableDictPrefForTesting,
          user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);

  base::Value::Dict local_value = base::Value::Dict()
                                      .Set("google.com", "allow")
                                      .Set("wikipedia.org", "allow");
  GetPrefs(0)->SetDict(sync_preferences::kSyncableMergeableDictPrefForTesting,
                       local_value.Clone());

  base::Value::Dict server_value = base::Value::Dict()
                                       .Set("facebook.com", "deny")
                                       .Set("microsoft.com", "deny")
                                       .Set("wikipedia.org", "deny");
  InjectPreferenceToFakeServer(
      syncer::PREFERENCES,
      sync_preferences::kSyncableMergeableDictPrefForTesting,
      base::Value(server_value.Clone()));

  // Enable Sync.
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::PREFERENCES));

  // Fake server value is synced to the account store and the dict value is
  // merged with the value in the local store.
  auto merged_value = base::Value::Dict()
                          .Set("facebook.com", "deny")
                          .Set("google.com", "allow")
                          .Set("microsoft.com", "deny")
                          .Set("wikipedia.org", "deny");
  EXPECT_EQ(GetPrefs(0)->GetDict(
                sync_preferences::kSyncableMergeableDictPrefForTesting),
            merged_value);

  // Disable syncing preferences.
  ASSERT_TRUE(GetClient(0)->DisableSelectableType(
      syncer::UserSelectableType::kPreferences));
  ASSERT_FALSE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::PREFERENCES));

  // The local store remains unchanged.
  EXPECT_EQ(GetPrefs(0)->GetDict(
                sync_preferences::kSyncableMergeableDictPrefForTesting),
            local_value);
}

IN_PROC_BROWSER_TEST_F(SingleClientPreferencesWithAccountStorageMergeSyncTest,
                       ShouldUnmergeMergeableDictPrefUponUpdate) {
  ASSERT_TRUE(SetupClients());
  ASSERT_FALSE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::PREFERENCES));

  GetRegistry(GetProfile(0))
      ->RegisterDictionaryPref(
          sync_preferences::kSyncableMergeableDictPrefForTesting,
          user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);

  base::Value::Dict local_value = base::Value::Dict()
                                      .Set("google.com", "allow")
                                      .Set("wikipedia.org", "allow");
  GetPrefs(0)->SetDict(sync_preferences::kSyncableMergeableDictPrefForTesting,
                       local_value.Clone());

  base::Value::Dict server_value = base::Value::Dict()
                                       .Set("facebook.com", "deny")
                                       .Set("microsoft.com", "deny")
                                       .Set("wikipedia.org", "deny");
  InjectPreferenceToFakeServer(
      syncer::PREFERENCES,
      sync_preferences::kSyncableMergeableDictPrefForTesting,
      base::Value(server_value.Clone()));

  // Enable Sync.
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::PREFERENCES));

  // Fake server value is synced to the account store and the dict value is
  // merged with the value in the local store.
  auto merged_value = base::Value::Dict()
                          .Set("facebook.com", "deny")
                          .Set("google.com", "allow")
                          .Set("microsoft.com", "deny")
                          .Set("wikipedia.org", "deny");
  ASSERT_EQ(GetPrefs(0)->GetDict(
                sync_preferences::kSyncableMergeableDictPrefForTesting),
            merged_value);

  auto updated_value = base::Value::Dict()
                           // New key, should get added to both stores.
                           .Set("cnn.com", "deny")
                           // Updated value, should get added to both stores.
                           .Set("facebook.com", "allow")
                           // Unchanged, should be only in the local store.
                           .Set("google.com", "allow")
                           // Unchanged, should be only in the account store.
                           .Set("microsoft.com", "deny");

  GetPrefs(0)->SetDict(sync_preferences::kSyncableMergeableDictPrefForTesting,
                       updated_value.Clone());
  ASSERT_EQ(GetPrefs(0)->GetDict(
                sync_preferences::kSyncableMergeableDictPrefForTesting),
            updated_value);

  // Note that entry for "wikipedia.org" was removed.
  auto updated_server_value = base::Value::Dict()
                                  .Set("cnn.com", "deny")
                                  .Set("facebook.com", "allow")
                                  .Set("microsoft.com", "deny");
  EXPECT_TRUE(FakeServerPrefMatchesValueChecker(
                  syncer::DataType::PREFERENCES,
                  sync_preferences::kSyncableMergeableDictPrefForTesting,
                  ConvertPrefValueToValueInSpecifics(
                      base::Value(updated_server_value.Clone())))
                  .Wait());

  // Disable syncing preferences.
  ASSERT_TRUE(GetClient(0)->DisableSelectableType(
      syncer::UserSelectableType::kPreferences));
  ASSERT_FALSE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::PREFERENCES));

  // Note that entry for "wikipedia.org" was removed.
  auto updated_local_value = base::Value::Dict()
                                 .Set("cnn.com", "deny")
                                 .Set("facebook.com", "allow")
                                 .Set("google.com", "allow");
  EXPECT_EQ(GetPrefs(0)->GetDict(
                sync_preferences::kSyncableMergeableDictPrefForTesting),
            updated_local_value);
}

IN_PROC_BROWSER_TEST_F(SingleClientPreferencesWithAccountStorageMergeSyncTest,
                       ShouldMergeLocalAndAccountMergeableListPref) {
  ASSERT_TRUE(SetupClients());
  // Register `kSyncableMergeableListPrefForTesting`.
  GetRegistry(GetProfile(0))
      ->RegisterListPref(sync_preferences::kSyncableMergeableListPrefForTesting,
                         user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  ASSERT_FALSE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::PREFERENCES));

  base::Value::List local_value =
      base::Value::List().Append("cnn.com").Append("facebook.com");
  GetPrefs(0)->SetList(sync_preferences::kSyncableMergeableListPrefForTesting,
                       local_value.Clone());

  base::Value::List server_value =
      base::Value::List().Append("google.com").Append("facebook.com");
  InjectPreferenceToFakeServer(
      syncer::PREFERENCES,
      sync_preferences::kSyncableMergeableListPrefForTesting,
      base::Value(server_value.Clone()));

  // Enable Sync.
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::PREFERENCES));

  // Fake server value is synced to the account store and the list value is
  // merged with the value in the local store.
  auto merged_value = base::Value::List()
                          .Append("google.com")
                          .Append("facebook.com")
                          .Append("cnn.com");
  EXPECT_EQ(GetPrefs(0)->GetList(
                sync_preferences::kSyncableMergeableListPrefForTesting),
            merged_value);

  // Disable syncing preferences.
  ASSERT_TRUE(GetClient(0)->DisableSelectableType(
      syncer::UserSelectableType::kPreferences));
  ASSERT_FALSE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::PREFERENCES));

  // The local store remains unchanged.
  EXPECT_EQ(GetPrefs(0)->GetList(
                sync_preferences::kSyncableMergeableListPrefForTesting),
            local_value);
}

IN_PROC_BROWSER_TEST_F(SingleClientPreferencesWithAccountStorageMergeSyncTest,
                       ShouldUnmergeMergeableListPrefUponUpdate) {
  ASSERT_TRUE(SetupClients());
  // Register `kSyncableMergeableListPrefForTesting`.
  GetRegistry(GetProfile(0))
      ->RegisterListPref(sync_preferences::kSyncableMergeableListPrefForTesting,
                         user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  ASSERT_FALSE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::PREFERENCES));

  base::Value::List local_value =
      base::Value::List().Append("cnn.com").Append("facebook.com");
  GetPrefs(0)->SetList(sync_preferences::kSyncableMergeableListPrefForTesting,
                       local_value.Clone());

  base::Value::List server_value =
      base::Value::List().Append("google.com").Append("facebook.com");
  InjectPreferenceToFakeServer(
      syncer::PREFERENCES,
      sync_preferences::kSyncableMergeableListPrefForTesting,
      base::Value(server_value.Clone()));

  // Enable Sync.
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::PREFERENCES));

  // Fake server value is synced to the account store and the list value is
  // merged with the value in the local store.
  auto merged_value = base::Value::List()
                          .Append("google.com")
                          .Append("facebook.com")
                          .Append("cnn.com");
  ASSERT_EQ(GetPrefs(0)->GetList(
                sync_preferences::kSyncableMergeableListPrefForTesting),
            merged_value);

  // Common entry for "facebook.com" is removed.
  auto updated_value =
      base::Value::List().Append("google.com").Append("cnn.com");
  GetPrefs(0)->SetList(sync_preferences::kSyncableMergeableListPrefForTesting,
                       updated_value.Clone());
  ASSERT_EQ(GetPrefs(0)->GetList(
                sync_preferences::kSyncableMergeableListPrefForTesting),
            updated_value);

  // No standard unmerging logic exists for list prefs and hence, updated value
  // is written to the account store and the local store.
  EXPECT_TRUE(FakeServerPrefMatchesValueChecker(
                  syncer::DataType::PREFERENCES,
                  sync_preferences::kSyncableMergeableListPrefForTesting,
                  ConvertPrefValueToValueInSpecifics(
                      base::Value(updated_value.Clone())))
                  .Wait());

  // Disable syncing preferences.
  ASSERT_TRUE(GetClient(0)->DisableSelectableType(
      syncer::UserSelectableType::kPreferences));
  ASSERT_FALSE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::PREFERENCES));

  // The local store remains unchanged.
  EXPECT_EQ(GetPrefs(0)->GetList(
                sync_preferences::kSyncableMergeableListPrefForTesting),
            updated_value);
}

#if BUILDFLAG(IS_ANDROID)

class SingleClientPreferencesWithMigrateAccountPrefsDisabledSyncTest
    : public SingleClientPreferencesWithAccountStorageSyncTest {
 public:
  SingleClientPreferencesWithMigrateAccountPrefsDisabledSyncTest() {
    feature_list_.InitAndDisableFeature(syncer::kMigrateAccountPrefs);
  }

  base::FilePath AccountPreferencesFilePath() const {
    return GetProfile(0)->GetPath().Append(chrome::kAccountPreferencesFilename);
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    SingleClientPreferencesWithMigrateAccountPrefsDisabledSyncTest,
    ShouldCreateAccountPrefsFile) {
  ASSERT_TRUE(SetupClients());
  // Register `sync_preferences::kSyncablePrefForTesting`.
  GetRegistry(GetProfile(0))
      ->RegisterStringPref(sync_preferences::kSyncablePrefForTesting, "",
                           user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  ASSERT_FALSE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::PREFERENCES));

  InjectPreferenceToFakeServer(syncer::PREFERENCES,
                               sync_preferences::kSyncablePrefForTesting,
                               base::Value("account value"));

  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::PREFERENCES));

  EXPECT_EQ(GetPrefs(0)->GetString(sync_preferences::kSyncablePrefForTesting),
            "account value");

  CommitToDiskAndWait();

  base::ScopedAllowBlockingForTesting allow_blocking;
  EXPECT_TRUE(base::PathExists(AccountPreferencesFilePath()));
}

IN_PROC_BROWSER_TEST_F(
    SingleClientPreferencesWithMigrateAccountPrefsDisabledSyncTest,
    ShouldCleanupAccountPreferencesFileOnDisable) {
  ASSERT_TRUE(SetupClients());
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
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::PREFERENCES));
  // Fake server value is synced to the account store and overrides local value.
  ASSERT_EQ(GetPrefs(0)->GetString(sync_preferences::kSyncablePrefForTesting),
            "account value");

  CommitToDiskAndWait();

  // Verify file content, `kSyncablePrefForTesting` is present.
  std::optional<base::Value::Dict> file_content = ReadValuesFromFile(
      GetProfile(0)->GetPath().Append(chrome::kAccountPreferencesFilename));
  ASSERT_TRUE(file_content.has_value());

  std::string* value =
      file_content->FindString(sync_preferences::kSyncablePrefForTesting);
  ASSERT_TRUE(value);
  EXPECT_EQ(*value, "account value");

  // Disable syncing preferences. This should lead to clearing of account prefs
  // file.
  ASSERT_TRUE(GetClient(0)->DisableSelectableType(
      syncer::UserSelectableType::kPreferences));
  ASSERT_FALSE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::PREFERENCES));
  ASSERT_EQ(GetPrefs(0)->GetString(sync_preferences::kSyncablePrefForTesting),
            "local value");

  CommitToDiskAndWait();

  // Account prefs have been removed from the file (but prefs with
  // kExemptFromUserControlWhileSignedIn may still be there).
  file_content = ReadValuesFromFile(
      GetProfile(0)->GetPath().Append(chrome::kAccountPreferencesFilename));
  ASSERT_TRUE(file_content.has_value());
  EXPECT_FALSE(
      file_content->FindString(sync_preferences::kSyncablePrefForTesting));
}

IN_PROC_BROWSER_TEST_F(
    SingleClientPreferencesWithMigrateAccountPrefsDisabledSyncTest,
    ShouldCleanupAccountPreferencesFileOnSignout) {
  ASSERT_TRUE(SetupClients());
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
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::PREFERENCES));
  // Fake server value is synced to the account store and overrides local value.
  ASSERT_EQ(GetPrefs(0)->GetString(sync_preferences::kSyncablePrefForTesting),
            "account value");

  CommitToDiskAndWait();

  // Verify file content, `kSyncablePrefForTesting` is present.
  std::optional<base::Value::Dict> file_content = ReadValuesFromFile(
      GetProfile(0)->GetPath().Append(chrome::kAccountPreferencesFilename));
  ASSERT_TRUE(file_content.has_value());

  std::string* value =
      file_content->FindString(sync_preferences::kSyncablePrefForTesting);
  ASSERT_TRUE(value);
  EXPECT_EQ(*value, "account value");

  // Signout. This should lead to clearing of account prefs file.
  GetClient(0)->SignOutPrimaryAccount();
  ASSERT_FALSE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::PREFERENCES));
  ASSERT_EQ(GetPrefs(0)->GetString(sync_preferences::kSyncablePrefForTesting),
            "local value");

  CommitToDiskAndWait();

  // Account prefs have been removed from the file.
  file_content = ReadValuesFromFile(
      GetProfile(0)->GetPath().Append(chrome::kAccountPreferencesFilename));
  ASSERT_TRUE(file_content.has_value());
  EXPECT_TRUE(file_content->empty());
}

class SingleClientPreferencesWithMigrateAccountPrefsEnabledSyncTest
    : public SingleClientPreferencesWithAccountStorageSyncTest {
 public:
  SingleClientPreferencesWithMigrateAccountPrefsEnabledSyncTest()
      : feature_list_(syncer::kMigrateAccountPrefs) {}

  base::FilePath AccountPreferencesFilePath() const {
    return GetProfile(0)->GetPath().Append(chrome::kAccountPreferencesFilename);
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    SingleClientPreferencesWithMigrateAccountPrefsEnabledSyncTest,
    ShouldNotCreateAccountPrefsFile) {
  ASSERT_TRUE(SetupClients());
  // Register `sync_preferences::kSyncablePrefForTesting`.
  GetRegistry(GetProfile(0))
      ->RegisterStringPref(sync_preferences::kSyncablePrefForTesting, "",
                           user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  ASSERT_FALSE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::PREFERENCES));

  InjectPreferenceToFakeServer(syncer::PREFERENCES,
                               sync_preferences::kSyncablePrefForTesting,
                               base::Value("account value"));

  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::PREFERENCES));

  EXPECT_EQ(GetPrefs(0)->GetString(sync_preferences::kSyncablePrefForTesting),
            "account value");

  CommitToDiskAndWait();

  base::ScopedAllowBlockingForTesting allow_blocking;
  EXPECT_FALSE(base::PathExists(AccountPreferencesFilePath()));
}

IN_PROC_BROWSER_TEST_F(
    SingleClientPreferencesWithMigrateAccountPrefsEnabledSyncTest,
    ShouldDownloadAccountPrefsWithInitialSync) {
  ASSERT_TRUE(SetupClients());
  // Register `sync_preferences::kSyncablePrefForTesting`.
  GetRegistry(GetProfile(0))
      ->RegisterStringPref(sync_preferences::kSyncablePrefForTesting, "",
                           user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  ASSERT_FALSE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::PREFERENCES));

  InjectPreferenceToFakeServer(syncer::PREFERENCES,
                               sync_preferences::kSyncablePrefForTesting,
                               base::Value("account value"));

  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::PREFERENCES));

  EXPECT_EQ(GetPrefs(0)->GetString(sync_preferences::kSyncablePrefForTesting),
            "account value");

  CommitToDiskAndWait();

  std::optional<base::Value::Dict> account_pref_values_on_disk =
      ReadValuesFromFile(
          GetProfile(0)->GetPath().Append(chrome::kPreferencesFilename),
          chrome_prefs::kAccountPreferencesPrefix);
  ASSERT_TRUE(account_pref_values_on_disk.has_value());
  std::string* value = account_pref_values_on_disk->FindStringByDottedPath(
      sync_preferences::kSyncablePrefForTesting);
  ASSERT_TRUE(value);
  EXPECT_EQ(*value, "account value");

  GetClient(0)->SignOutPrimaryAccount();
  ASSERT_FALSE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::PREFERENCES));

  ASSERT_FALSE(
      GetPrefs(0)->GetUserPrefValue(sync_preferences::kSyncablePrefForTesting));
  CommitToDiskAndWait();
  account_pref_values_on_disk = ReadValuesFromFile(
      GetProfile(0)->GetPath().Append(chrome::kPreferencesFilename),
      chrome_prefs::kAccountPreferencesPrefix);
  ASSERT_TRUE(account_pref_values_on_disk.has_value());
  EXPECT_TRUE(account_pref_values_on_disk->empty());
}

IN_PROC_BROWSER_TEST_F(
    SingleClientPreferencesWithMigrateAccountPrefsEnabledSyncTest,
    ShouldCleanupAccountPreferencesFileOnDisable) {
  ASSERT_TRUE(SetupClients());
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
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::PREFERENCES));
  // Fake server value is synced to the account store and overrides local value.
  ASSERT_EQ(GetPrefs(0)->GetString(sync_preferences::kSyncablePrefForTesting),
            "account value");

  CommitToDiskAndWait();

  // Verify file content, `kSyncablePrefForTesting` is present.
  std::optional<base::Value::Dict> file_content = ReadValuesFromFile(
      GetProfile(0)->GetPath().Append(chrome::kPreferencesFilename),
      chrome_prefs::kAccountPreferencesPrefix);
  ASSERT_TRUE(file_content.has_value());

  std::string* value =
      file_content->FindString(sync_preferences::kSyncablePrefForTesting);
  ASSERT_TRUE(value);
  EXPECT_EQ(*value, "account value");

  // Disable syncing preferences. This should lead to clearing of account prefs
  // file.
  ASSERT_TRUE(GetClient(0)->DisableSelectableType(
      syncer::UserSelectableType::kPreferences));
  ASSERT_FALSE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::PREFERENCES));
  ASSERT_EQ(GetPrefs(0)->GetString(sync_preferences::kSyncablePrefForTesting),
            "local value");

  CommitToDiskAndWait();

  // Account prefs have been removed from the file (but prefs with
  // kExemptFromUserControlWhileSignedIn may still be there).
  file_content = ReadValuesFromFile(
      GetProfile(0)->GetPath().Append(chrome::kPreferencesFilename),
      chrome_prefs::kAccountPreferencesPrefix);
  ASSERT_TRUE(file_content.has_value());
  EXPECT_FALSE(
      file_content->FindString(sync_preferences::kSyncablePrefForTesting));
}

IN_PROC_BROWSER_TEST_F(
    SingleClientPreferencesWithMigrateAccountPrefsEnabledSyncTest,
    ShouldCleanupAccountPreferencesFileOnSignout) {
  ASSERT_TRUE(SetupClients());
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
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::PREFERENCES));
  // Fake server value is synced to the account store and overrides local value.
  ASSERT_EQ(GetPrefs(0)->GetString(sync_preferences::kSyncablePrefForTesting),
            "account value");

  CommitToDiskAndWait();

  // Verify file content, `kSyncablePrefForTesting` is present.
  std::optional<base::Value::Dict> file_content = ReadValuesFromFile(
      GetProfile(0)->GetPath().Append(chrome::kPreferencesFilename),
      chrome_prefs::kAccountPreferencesPrefix);
  ASSERT_TRUE(file_content.has_value());

  std::string* value =
      file_content->FindString(sync_preferences::kSyncablePrefForTesting);
  ASSERT_TRUE(value);
  EXPECT_EQ(*value, "account value");

  // Signout. This should lead to clearing of account prefs file.
  GetClient(0)->SignOutPrimaryAccount();
  ASSERT_FALSE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::PREFERENCES));
  ASSERT_EQ(GetPrefs(0)->GetString(sync_preferences::kSyncablePrefForTesting),
            "local value");

  CommitToDiskAndWait();

  // Account prefs have been removed from the file.
  file_content = ReadValuesFromFile(
      GetProfile(0)->GetPath().Append(chrome::kPreferencesFilename),
      chrome_prefs::kAccountPreferencesPrefix);
  ASSERT_TRUE(file_content.has_value());
  EXPECT_TRUE(file_content->empty());
}

// TODO(crbug.com/346508597): Add a test
// ShouldDownloadAccountPrefsFromSavedSyncData to test that account data is
// loaded from the sync's copy upon startup. This test has not been added due to
// lack of support for PRE_ tests in Android.

#endif  // BUILDFLAG(IS_ANDROID)

// Preference tracking is not required on android and chromeos.
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)

const char* kProtectedPrefName = prefs::kShowHomeButton;
const char* kUnprotectedPrefName = prefs::kShowForwardButton;

class SingleClientTrackedPreferencesSyncTest
    : public SingleClientPreferencesWithAccountStorageSyncTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    SingleClientPreferencesWithAccountStorageSyncTest::
        SetUpInProcessBrowserTestFixture();

    // Bots are on a domain, turn off the domain check for settings hardening in
    // order to be able to test all SettingsEnforcement groups.
    chrome_prefs::DisableDomainCheckForTesting();
  }

  bool IsProtectionEnforced() const {
// Only windows and mac have the strongest enforcement setting.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
    return true;
#else
    return false;
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  }
};

IN_PROC_BROWSER_TEST_F(SingleClientTrackedPreferencesSyncTest,
                       ShouldStoreUnprotectedPrefsInPreferencesFile) {
  ASSERT_TRUE(SetupClients());
  ASSERT_FALSE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::PREFERENCES));

  // Default value.
  ASSERT_TRUE(GetPrefs(0)->GetBoolean(kUnprotectedPrefName));

  GetPrefs(0)->SetBoolean(kUnprotectedPrefName, false);
  InjectPreferenceToFakeServer(syncer::PREFERENCES, kUnprotectedPrefName,
                               base::Value(true));

  // Enable Sync.
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::PREFERENCES));

  // Fake server value is synced to the account store and overrides local value.
  ASSERT_TRUE(GetPrefs(0)->GetBoolean(kUnprotectedPrefName));

  CommitToDiskAndWait();

  // Check unprotected pref is present in the main preference file.
  std::string account_pref_name = base::StringPrintf(
      "%s.%s", chrome_prefs::kAccountPreferencesPrefix, kUnprotectedPrefName);
  std::optional<base::Value::Dict> prefs = ReadValuesFromFile(
      GetProfile(0)->GetPath().Append(chrome::kPreferencesFilename));
  ASSERT_TRUE(prefs.has_value());
  ASSERT_TRUE(prefs->FindByDottedPath(kUnprotectedPrefName))
      << "Missing key " << kUnprotectedPrefName << " in " << *prefs;
  EXPECT_TRUE(prefs->FindByDottedPath(account_pref_name))
      << "Missing key " << account_pref_name << " in " << *prefs;

  // Check unprotected pref is not in the secured preference file.
  std::optional<base::Value::Dict> secured_prefs = ReadValuesFromFile(
      GetProfile(0)->GetPath().Append(chrome::kSecurePreferencesFilename));
  ASSERT_TRUE(secured_prefs.has_value());
  ASSERT_FALSE(secured_prefs->FindByDottedPath(kUnprotectedPrefName))
      << "Incorrect key " << kUnprotectedPrefName << " in " << *secured_prefs;
  EXPECT_FALSE(secured_prefs->FindByDottedPath(account_pref_name))
      << "Incorrect key " << account_pref_name << " in " << *secured_prefs;
}

IN_PROC_BROWSER_TEST_F(SingleClientTrackedPreferencesSyncTest,
                       ShouldStoreProtectedPrefsInCorrectPreferencesFile) {
  ASSERT_TRUE(SetupClients());
  ASSERT_FALSE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::PREFERENCES));

  // Default value.
  ASSERT_FALSE(GetPrefs(0)->GetBoolean(kProtectedPrefName));

  GetPrefs(0)->SetBoolean(kProtectedPrefName, true);
  InjectPreferenceToFakeServer(syncer::PREFERENCES, kProtectedPrefName,
                               base::Value(false));

  // Enable Sync.
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::PREFERENCES));

  // Fake server value is synced to the account store and overrides local value.
  ASSERT_FALSE(GetPrefs(0)->GetBoolean(kProtectedPrefName));

  CommitToDiskAndWait();

  std::string account_pref_name = base::StringPrintf(
      "%s.%s", chrome_prefs::kAccountPreferencesPrefix, kProtectedPrefName);
  if (!IsProtectionEnforced()) {
    // Check protected pref is present in the main preference file.
    std::optional<base::Value::Dict> prefs = ReadValuesFromFile(
        GetProfile(0)->GetPath().Append(chrome::kPreferencesFilename));
    ASSERT_TRUE(prefs.has_value());
    ASSERT_TRUE(prefs->FindByDottedPath(kProtectedPrefName))
        << "Missing key " << kProtectedPrefName << " in " << *prefs;
    EXPECT_TRUE(prefs->FindByDottedPath(account_pref_name))
        << "Missing key " << account_pref_name << " in " << *prefs;
    return;
  }

  // Check protected pref is not in the main preference file.
  std::optional<base::Value::Dict> prefs = ReadValuesFromFile(
      GetProfile(0)->GetPath().Append(chrome::kPreferencesFilename));
  ASSERT_TRUE(prefs.has_value());
  ASSERT_FALSE(prefs->FindByDottedPath(kProtectedPrefName))
      << "Incorrect key " << kProtectedPrefName << " in " << *prefs;
  EXPECT_FALSE(prefs->FindByDottedPath(account_pref_name))
      << "Incorrect key " << account_pref_name << " in " << *prefs;

  // Check protected pref is present in the secured preference file.
  std::optional<base::Value::Dict> secured_prefs = ReadValuesFromFile(
      GetProfile(0)->GetPath().Append(chrome::kSecurePreferencesFilename));
  ASSERT_TRUE(secured_prefs.has_value());
  ASSERT_TRUE(secured_prefs->FindByDottedPath(kProtectedPrefName))
      << "Missing key " << kProtectedPrefName << " in " << *secured_prefs;
  EXPECT_TRUE(secured_prefs->FindByDottedPath(account_pref_name))
      << "Missing key " << account_pref_name << " in " << *secured_prefs;
}

IN_PROC_BROWSER_TEST_F(SingleClientTrackedPreferencesSyncTest,
                       ShouldHashTrackedSyncablePrefs) {
  const char kHashesPrefName[] = "protection.macs";

  ASSERT_TRUE(SetupClients());
  ASSERT_FALSE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::PREFERENCES));

  // Default value.
  ASSERT_FALSE(GetPrefs(0)->GetBoolean(kProtectedPrefName));

  GetPrefs(0)->SetBoolean(kProtectedPrefName, true);
  InjectPreferenceToFakeServer(syncer::PREFERENCES, kProtectedPrefName,
                               base::Value(false));

  // Enable Sync.
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::PREFERENCES));

  // Fake server value is synced to the account store and overrides local value.
  ASSERT_FALSE(GetPrefs(0)->GetBoolean(kProtectedPrefName));

  CommitToDiskAndWait();

  // Load hashes from the preference or the secured preference file.
  std::optional<base::Value::Dict> protection_values = ReadValuesFromFile(
      GetProfile(0)->GetPath().Append((IsProtectionEnforced()
                                           ? chrome::kSecurePreferencesFilename
                                           : chrome::kPreferencesFilename)),
      kHashesPrefName);
  ASSERT_TRUE(protection_values.has_value());

  // There should be hashes for both, the regular and the account-prefixed pref
  // name.
  std::string account_pref_name = base::StringPrintf(
      "%s.%s", chrome_prefs::kAccountPreferencesPrefix, kProtectedPrefName);
  ASSERT_TRUE(protection_values->FindByDottedPath(kProtectedPrefName))
      << "Missing key " << kProtectedPrefName << " in "
      << protection_values.value();
  EXPECT_TRUE(protection_values->FindByDottedPath(account_pref_name))
      << "Missing key " << account_pref_name << " in "
      << protection_values.value();
}

IN_PROC_BROWSER_TEST_F(SingleClientTrackedPreferencesSyncTest,
                       PRE_ShouldLoadTrackedSyncablePrefsBeforeSyncStart) {
  ASSERT_TRUE(SetupClients());
  ASSERT_FALSE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::PREFERENCES));

  // Default value.
  ASSERT_FALSE(GetPrefs(0)->GetBoolean(kProtectedPrefName));

  InjectPreferenceToFakeServer(syncer::PREFERENCES, kProtectedPrefName,
                               base::Value(true));

  // Enable Sync.
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::PREFERENCES));

  // Fake server value is synced to the account store and overrides local value.
  ASSERT_TRUE(GetPrefs(0)->GetBoolean(kProtectedPrefName));
}

IN_PROC_BROWSER_TEST_F(SingleClientTrackedPreferencesSyncTest,
                       ShouldLoadTrackedSyncablePrefsBeforeSyncStart) {
  ASSERT_TRUE(SetupClients());
  ASSERT_FALSE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::PREFERENCES));

  // Account value loaded before sync starts.
  EXPECT_TRUE(GetPrefs(0)->GetBoolean(kProtectedPrefName));

  // Check protected account pref is present in the preference file.
  std::string account_pref_name = base::StringPrintf(
      "%s.%s", chrome_prefs::kAccountPreferencesPrefix, kProtectedPrefName);
  std::optional<base::Value::Dict> prefs =
      ReadValuesFromFile(GetProfile(0)->GetPath().Append(
          (IsProtectionEnforced() ? chrome::kSecurePreferencesFilename
                                  : chrome::kPreferencesFilename)));
  ASSERT_TRUE(prefs.has_value());
  EXPECT_TRUE(prefs->FindByDottedPath(account_pref_name))
      << "Missing key " << account_pref_name << " in " << *prefs;
}

class SingleClientTrackedPreferencesSyncTestWithAttack
    : public SingleClientTrackedPreferencesSyncTest {
 public:
  bool SetUpUserDataDirectory() override {
    EXPECT_TRUE(
        SingleClientTrackedPreferencesSyncTest::SetUpUserDataDirectory());

    if (!content::IsPreTest()) {
      AttackTrackedSyncablePreference();
    }
    return true;
  }

  void AttackTrackedSyncablePreference() {
    const auto& filename = IsProtectionEnforced()
                               ? chrome::kSecurePreferencesFilename
                               : chrome::kPreferencesFilename;

    base::FilePath user_data_dir;
    ASSERT_TRUE(base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir));
    base::FilePath file_path =
        user_data_dir.Append(GetProfileBaseName(0)).Append(filename);

    std::optional<base::Value::Dict> values = ReadValuesFromFile(file_path);
    ASSERT_TRUE(values.has_value());
    // Update the existing account value.
    std::string pref_name = base::StringPrintf(
        "%s.%s", chrome_prefs::kAccountPreferencesPrefix, kProtectedPrefName);
    std::optional<bool> current_value = values->FindBoolByDottedPath(pref_name);
    ASSERT_TRUE(current_value.has_value())
        << "Missing key " << pref_name << " in " << values.value();
    values->SetByDottedPath(pref_name, !current_value.value());
    JSONFileValueSerializer serializer(file_path);
    EXPECT_TRUE(serializer.Serialize(*values));
  }

 protected:
  base::HistogramTester histogram_tester_;
};

IN_PROC_BROWSER_TEST_F(SingleClientTrackedPreferencesSyncTestWithAttack,
                       PRE_ShouldProtectTrackedSyncablePrefs) {
  ASSERT_TRUE(SetupClients());
  ASSERT_FALSE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::PREFERENCES));

  // Default value.
  ASSERT_FALSE(GetPrefs(0)->GetBoolean(kProtectedPrefName));

  GetPrefs(0)->SetBoolean(kProtectedPrefName, true);
  InjectPreferenceToFakeServer(syncer::PREFERENCES, kProtectedPrefName,
                               base::Value(false));

  // Enable Sync.
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::PREFERENCES));

  // Fake server value is synced to the account store and overrides local value.
  ASSERT_FALSE(GetPrefs(0)->GetBoolean(kProtectedPrefName));
}

IN_PROC_BROWSER_TEST_F(SingleClientTrackedPreferencesSyncTestWithAttack,
                       ShouldProtectTrackedSyncablePrefs) {
  ASSERT_TRUE(SetupClients());
  ASSERT_FALSE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::PREFERENCES));

  // ID for show home page pref is 0.
  EXPECT_EQ(histogram_tester_.GetBucketCount(
                "Settings.TrackedPreferenceWantedReset", /*sample=*/0),
            static_cast<int>(!IsProtectionEnforced()));
  EXPECT_EQ(histogram_tester_.GetBucketCount("Settings.TrackedPreferenceReset",
                                             /*sample=*/0),
            static_cast<int>(IsProtectionEnforced()));

  if (!IsProtectionEnforced()) {
    return;
  }

  // Tracked account pref should be reset after attack. So the local value takes
  // effect.
  EXPECT_TRUE(GetPrefs(0)->GetBoolean(kProtectedPrefName));

  CommitToDiskAndWait();

  // Check protected account pref is no longer present in the preference file.
  std::string account_pref_name = base::StringPrintf(
      "%s.%s", chrome_prefs::kAccountPreferencesPrefix, kProtectedPrefName);
  std::optional<base::Value::Dict> prefs = ReadValuesFromFile(
      GetProfile(0)->GetPath().Append(chrome::kSecurePreferencesFilename));
  ASSERT_TRUE(prefs.has_value());
  EXPECT_FALSE(prefs->FindByDottedPath(account_pref_name))
      << "Incorrect key " << account_pref_name << " in " << *prefs;
}

#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(ENABLE_GLIC)

class SingleClientPreferencesGlicTieredRolloutTest
    : public SingleClientPreferencesSyncTest {
 public:
  SingleClientPreferencesGlicTieredRolloutTest() {
    feature_list_.InitWithFeatures({::features::kGlicTieredRollout},
                                   {::features::kGlicRollout});
  }

  void SetGlicTieredRolloutEligibility(bool is_eligible) {
    InjectPreferenceToFakeServer(syncer::PRIORITY_PREFERENCES,
                                 glic::prefs::kGlicRolloutEligibility,
                                 base::Value(is_eligible));
  }

 private:
  glic::GlicTestEnvironment glic_test_env_{
      {.force_signin_and_model_execution_capability = false}};
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(SingleClientPreferencesGlicTieredRolloutTest, E2E) {
  ASSERT_TRUE(SetupClients());

  // Have user be eligible for Glic from an account perspective.
  ASSERT_TRUE(GetClient(0)->SignInPrimaryAccount());
  glic::SetModelExecutionCapability(GetProfile(0), /*enabled=*/true);

  // Should not be enabled as profile not eligible for tiered rollout.
  EXPECT_FALSE(glic::GlicEnabling::IsEnabledForProfile(GetProfile(0)));

  // Set user eligible via server.
  SetGlicTieredRolloutEligibility(/*is_eligible=*/true);
  ASSERT_TRUE(SetupSync());

  // User should have priority preferences synced and rollout eligibility is
  // true.
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(
      syncer::PRIORITY_PREFERENCES));
  EXPECT_TRUE(GetPrefs(0)->GetBoolean(glic::prefs::kGlicRolloutEligibility));
  EXPECT_TRUE(glic::GlicEnabling::IsEnabledForProfile(GetProfile(0)));
}

#endif  // BUILDFLAG(ENABLE_GLIC)

class SingleClientPreferencesSubscriptionEligibilityTest
    : public SingleClientPreferencesSyncTest {
 public:
  void SetAiSubscriptionTier(int32_t subscription_tier) {
    InjectPreferenceToFakeServer(
        syncer::PRIORITY_PREFERENCES,
        subscription_eligibility::prefs::kAiSubscriptionTier,
        base::Value(subscription_tier));
  }

  subscription_eligibility::SubscriptionEligibilityService* service() {
    return subscription_eligibility::SubscriptionEligibilityServiceFactory::
        GetForProfile(GetProfile(0));
  }
};

IN_PROC_BROWSER_TEST_F(SingleClientPreferencesSubscriptionEligibilityTest,
                       E2E) {
  ASSERT_TRUE(SetupClients());

  // Should just have 0 as the default value.
  EXPECT_EQ(0, service()->GetAiSubscriptionTier());

  // Set user eligible via server.
  SetAiSubscriptionTier(/*subscription_tier=*/1);
  ASSERT_TRUE(SetupSync());

  // User should have priority preferences synced and rollout eligibility is
  // true.
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(
      syncer::PRIORITY_PREFERENCES));
  EXPECT_EQ(1, GetPrefs(0)->GetInteger(
                   subscription_eligibility::prefs::kAiSubscriptionTier));
  EXPECT_EQ(1, service()->GetAiSubscriptionTier());
}

class SingleClientDecouplePriorityPreferencesSyncTestWithFlagDisabled
    : public SingleClientPreferencesWithAccountStorageSyncTest {
 public:
  SingleClientDecouplePriorityPreferencesSyncTestWithFlagDisabled() {
    feature_list_.InitAndDisableFeature(
        syncer::kSyncSupportAlwaysSyncingPriorityPreferences);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    SingleClientDecouplePriorityPreferencesSyncTestWithFlagDisabled,
    InactiveWhenUserToggleIsOff) {
  ASSERT_TRUE(SetupClients());
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().empty());

  ASSERT_TRUE(GetClient(0)->SignInPrimaryAccount());
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  ASSERT_TRUE(GetSyncService(0)->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kPreferences));
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::PREFERENCES));
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(
      syncer::PRIORITY_PREFERENCES));

  // Disable all user selectable types.
  GetSyncService(0)->GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false, syncer::UserSelectableTypeSet());
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());

  // User toggle is off.
  ASSERT_FALSE(GetSyncService(0)->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kPreferences));
  // Preferences and Priority preferences are inactive.
  ASSERT_FALSE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::PREFERENCES));
  EXPECT_FALSE(GetSyncService(0)->GetActiveDataTypes().Has(
      syncer::PRIORITY_PREFERENCES));
}

class SingleClientDecouplePriorityPreferencesSyncTest
    : public SingleClientPreferencesWithAccountStorageSyncTest {
 private:
  base::test::ScopedFeatureList feature_list_{
      syncer::kSyncSupportAlwaysSyncingPriorityPreferences};
};

IN_PROC_BROWSER_TEST_F(SingleClientDecouplePriorityPreferencesSyncTest,
                       ActiveWhenUserToggleIsOff) {
  ASSERT_TRUE(SetupClients());
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().empty());

  ASSERT_TRUE(GetClient(0)->SignInPrimaryAccount());
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  ASSERT_TRUE(GetSyncService(0)->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kPreferences));
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::PREFERENCES));
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(
      syncer::PRIORITY_PREFERENCES));

  // Disable all user selectable types.
  GetSyncService(0)->GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false, syncer::UserSelectableTypeSet());
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());

  // User toggle is off.
  ASSERT_FALSE(GetSyncService(0)->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kPreferences));
  // Preferences is inactive.
  ASSERT_FALSE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::PREFERENCES));
  // Priority preferences is still active.
  EXPECT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(
      syncer::PRIORITY_PREFERENCES));
}

IN_PROC_BROWSER_TEST_F(SingleClientDecouplePriorityPreferencesSyncTest,
                       ShouldSyncAllowlistedPriorityPrefWithoutOptIn) {
  ASSERT_TRUE(SetupClients());
  // Regular priority pref.
  GetRegistry(GetProfile(0))
      ->RegisterStringPref(
          sync_preferences::kSyncablePriorityPrefForTesting, "",
          user_prefs::PrefRegistrySyncable::SYNCABLE_PRIORITY_PREF);
  // Always syncing priority pref.
  GetRegistry(GetProfile(0))
      ->RegisterStringPref(
          sync_preferences::kSyncableAlwaysSyncingPriorityPrefForTesting, "",
          user_prefs::PrefRegistrySyncable::SYNCABLE_PRIORITY_PREF);

  ASSERT_FALSE(GetSyncService(0)->GetActiveDataTypes().Has(
      syncer::PRIORITY_PREFERENCES));

  ASSERT_TRUE(GetClient(0)->SignInPrimaryAccount());
  // Disable all user selectable types.
  GetSyncService(0)->GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false, syncer::UserSelectableTypeSet());
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());

  ASSERT_FALSE(GetSyncService(0)->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kPreferences));
  // Priority preferences is still active.
  EXPECT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(
      syncer::PRIORITY_PREFERENCES));

  InjectPreferenceToFakeServer(
      syncer::PRIORITY_PREFERENCES,
      sync_preferences::kSyncablePriorityPrefForTesting, base::Value("value1"));
  InjectPreferenceToFakeServer(
      syncer::PRIORITY_PREFERENCES,
      sync_preferences::kSyncableAlwaysSyncingPriorityPrefForTesting,
      base::Value("value1"));

  // Only the allowlisted priority pref is synced.
  EXPECT_TRUE(
      PrefValueChecker(
          GetPrefs(0),
          sync_preferences::kSyncableAlwaysSyncingPriorityPrefForTesting,
          base::Value("value1"))
          .Wait());
  // The regular priority pref is not synced.
  EXPECT_EQ(
      GetPrefs(0)->GetString(sync_preferences::kSyncablePriorityPrefForTesting),
      "");

  GetPrefs(0)->SetString(sync_preferences::kSyncablePriorityPrefForTesting,
                         "value2");
  GetPrefs(0)->SetString(
      sync_preferences::kSyncableAlwaysSyncingPriorityPrefForTesting, "value2");
  EXPECT_TRUE(
      FakeServerPrefMatchesValueChecker(
          syncer::PRIORITY_PREFERENCES,
          sync_preferences::kSyncableAlwaysSyncingPriorityPrefForTesting,
          ConvertPrefValueToValueInSpecifics(base::Value("value2")))
          .Wait());
  // The regular priority pref is not synced, remote is set to the old value.
  EXPECT_THAT(
      preferences_helper::GetPreferenceInFakeServer(
          syncer::PRIORITY_PREFERENCES,
          sync_preferences::kSyncablePriorityPrefForTesting, GetFakeServer())
          ->value(),
      ConvertPrefValueToValueInSpecifics(base::Value("value1")));
}

IN_PROC_BROWSER_TEST_F(SingleClientDecouplePriorityPreferencesSyncTest,
                       ShouldSyncRegularPriorityPrefWithOptIn) {
  ASSERT_TRUE(SetupClients());
  // Regular syncable pref.
  GetRegistry(GetProfile(0))
      ->RegisterStringPref(
          sync_preferences::kSyncablePriorityPrefForTesting, "",
          user_prefs::PrefRegistrySyncable::SYNCABLE_PRIORITY_PREF);

  ASSERT_TRUE(GetClient(0)->SignInPrimaryAccount());
  // Disable all user selectable types except preferences.
  GetSyncService(0)->GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      syncer::UserSelectableTypeSet(
          {syncer::UserSelectableType::kPreferences}));
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());

  ASSERT_TRUE(GetSyncService(0)->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kPreferences));
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(
      syncer::PRIORITY_PREFERENCES));

  InjectPreferenceToFakeServer(
      syncer::PRIORITY_PREFERENCES,
      sync_preferences::kSyncablePriorityPrefForTesting, base::Value("value1"));

  // The regular syncable pref is  synced.
  EXPECT_TRUE(
      PrefValueChecker(GetPrefs(0),
                       sync_preferences::kSyncablePriorityPrefForTesting,
                       base::Value("value1"))
          .Wait());

  GetPrefs(0)->SetString(sync_preferences::kSyncablePriorityPrefForTesting,
                         "value2");
  // The regular syncable pref is  synced.
  EXPECT_TRUE(FakeServerPrefMatchesValueChecker(
                  syncer::PRIORITY_PREFERENCES,
                  sync_preferences::kSyncablePriorityPrefForTesting,
                  ConvertPrefValueToValueInSpecifics(base::Value("value2")))
                  .Wait());
}

#if !BUILDFLAG(IS_ANDROID)
// Regression test for crbug.com/415305009.
class SingleClientFeatureListEarlyAccessTest
    : public SingleClientDecouplePriorityPreferencesSyncTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    SingleClientDecouplePriorityPreferencesSyncTest::
        SetUpInProcessBrowserTestFixture();
    policy_provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);

    policy::PolicyMap policies;
    base::Value::List clear_browsing_data_list =
        base::Value::List().Append("site_settings");
    policies.Set(policy::key::kClearBrowsingDataOnExitList,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD,
                 base::Value(std::move(clear_browsing_data_list)), nullptr);

    policy_provider_.UpdateChromePolicy(policies);
  }

 protected:
  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;
};

IN_PROC_BROWSER_TEST_F(SingleClientFeatureListEarlyAccessTest,
                       ShouldNotCrashUponEarlyFeatureAccess) {
  ASSERT_TRUE(SetupClients());
}
#endif  // !BUILDFLAG(IS_ANDROID)

// PRE_ tests are not supported on Android.
#if !BUILDFLAG(IS_ANDROID)
// ChromeOS does not support sign-in allowed flag.
#if !BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(
    SingleClientPreferencesWithAccountStorageSyncTest,
    PRE_ShouldClearAccountDataOnStartupIfSignInAllowedBitChanged) {
  ASSERT_TRUE(SetupClients());
  GetRegistry(GetProfile(0))
      ->RegisterStringPref(sync_preferences::kSyncablePrefForTesting, "",
                           user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);

  // Set the sign-in allowed bit to true initially.
  preferences_helper::GetPrefs(/*index=*/0)
      ->SetBoolean(prefs::kSigninAllowedOnNextStartup, true);

  // Set local value of the test pref.
  preferences_helper::ChangeStringPref(
      0, sync_preferences::kSyncablePrefForTesting, "local value");

  InjectPreferenceToFakeServer(syncer::PREFERENCES,
                               sync_preferences::kSyncablePrefForTesting,
                               base::Value("account value"));

  ASSERT_TRUE(GetClient(0)->SignInPrimaryAccount());
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());

  // Account value is effective.
  ASSERT_EQ(GetPrefs(0)->GetString(sync_preferences::kSyncablePrefForTesting),
            "account value");

  // Simulate turning off the sign-in allowed bit on the settings page.
  preferences_helper::GetPrefs(/*index=*/0)
      ->SetBoolean(prefs::kSigninAllowedOnNextStartup, false);
}

IN_PROC_BROWSER_TEST_F(
    SingleClientPreferencesWithAccountStorageSyncTest,
    ShouldClearAccountDataOnStartupIfSignInAllowedBitChanged) {
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(SetupClients());
  GetRegistry(GetProfile(0))
      ->RegisterStringPref(sync_preferences::kSyncablePrefForTesting, "",
                           user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);

  // Original local value should get re-applied.
  // Note: The account value is not cleared instantaneously upon startup, but
  // upon preferences sync initialization.
  EXPECT_TRUE(PrefValueChecker(GetPrefs(0),
                               sync_preferences::kSyncablePrefForTesting,
                               base::Value("local value"))
                  .Wait());
  // Clearing happened with StayStoppedAndMaybeClearData() called upon startup.
  histogram_tester.ExpectUniqueSample(
      "Sync.SyncableService.MaybeClearDataIfMetadataEmptyOrInvalid.PREFERENCE",
      true,
      /*expected_bucket_count=*/1);
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

IN_PROC_BROWSER_TEST_F(
    SingleClientPreferencesWithAccountStorageSyncTest,
    PRE_ShouldClearAccountDataOnStartupIfAccountStateChanged) {
  ASSERT_TRUE(SetupClients());
  GetRegistry(GetProfile(0))
      ->RegisterStringPref(sync_preferences::kSyncablePrefForTesting, "",
                           user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);

  // Set local value of the test pref.
  preferences_helper::ChangeStringPref(
      0, sync_preferences::kSyncablePrefForTesting, "local value");

  InjectPreferenceToFakeServer(syncer::PREFERENCES,
                               sync_preferences::kSyncablePrefForTesting,
                               base::Value("account value"));

#if BUILDFLAG(IS_CHROMEOS)
  ASSERT_TRUE(SetupSync());
#else
  ASSERT_TRUE(GetClient(0)->SignInPrimaryAccount());
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
#endif  // BUILDFLAG(IS_CHROMEOS)

  // Account value is effective.
  ASSERT_EQ(GetPrefs(0)->GetString(sync_preferences::kSyncablePrefForTesting),
            "account value");

  // Simulate a data type error to prevent clearing of account data.
  GetSyncService(0)->ReportDataTypeErrorForTest(syncer::PREFERENCES);
#if BUILDFLAG(IS_CHROMEOS)
  // Disable sync.
  ASSERT_TRUE(GetClient(0)->DisableSelectableType(
      syncer::UserSelectableType::kPreferences));
#else
  // Sign out. Account value should stay because of the data type error.
  GetClient(0)->SignOutPrimaryAccount();
#endif  // BUILDFLAG(IS_CHROMEOS)
  // Local value is not restored because stop sync is not called when there is a
  // data type error.
  ASSERT_EQ(GetPrefs(0)->GetString(sync_preferences::kSyncablePrefForTesting),
            "account value");

  ExcludeDataTypesFromCheckForDataTypeFailures({syncer::PREFERENCES});
}

IN_PROC_BROWSER_TEST_F(SingleClientPreferencesWithAccountStorageSyncTest,
                       ShouldClearAccountDataOnStartupIfAccountStateChanged) {
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(SetupClients());
  GetRegistry(GetProfile(0))
      ->RegisterStringPref(sync_preferences::kSyncablePrefForTesting, "",
                           user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);

  // Original local value should get re-applied.
  // Note: The account value is not cleared instantaneously upon startup, but
  // upon preferences sync initialization.
  EXPECT_TRUE(PrefValueChecker(GetPrefs(0),
                               sync_preferences::kSyncablePrefForTesting,
                               base::Value("local value"))
                  .Wait());
  // Clearing happened with StayStoppedAndMaybeClearData() called upon startup.
  histogram_tester.ExpectUniqueSample(
      "Sync.SyncableService.MaybeClearDataIfMetadataEmptyOrInvalid.PREFERENCE",
      true,
      /*expected_bucket_count=*/1);
}
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace
