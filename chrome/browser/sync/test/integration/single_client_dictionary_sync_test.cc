// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/spellchecker/spellcheck_custom_dictionary.h"
#include "chrome/browser/sync/test/integration/dictionary_helper.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/updated_progress_marker_checker.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/sync/base/features.h"
#include "components/sync/service/sync_service_impl.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {

using testing::ElementsAre;
using testing::IsEmpty;
using testing::UnorderedElementsAre;

constexpr char kLocalWord[] = "local";
constexpr char kAccountWord[] = "account";

class SingleClientDictionarySyncTest
    : public SyncTest,
      public testing::WithParamInterface<bool> {
 public:
  SingleClientDictionarySyncTest() : SyncTest(SINGLE_CLIENT) {
    feature_list_.InitWithFeatureState(
        syncer::kSpellcheckSeparateLocalAndAccountDictionaries, GetParam());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(SingleClientDictionarySyncTest, Sanity) {
  ASSERT_TRUE(SetupSync());
  dictionary_helper::LoadDictionaries();
  EXPECT_THAT(dictionary_helper::GetDictionaryWords(0), IsEmpty());

  const std::string word = "foo";
  EXPECT_TRUE(dictionary_helper::AddWord(0, word));
  EXPECT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());
  EXPECT_THAT(dictionary_helper::GetDictionaryWords(0), ElementsAre(word));

  EXPECT_TRUE(dictionary_helper::RemoveWord(0, word));
  EXPECT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());
  EXPECT_THAT(dictionary_helper::GetDictionaryWords(0), IsEmpty());
}

INSTANTIATE_TEST_SUITE_P(, SingleClientDictionarySyncTest, ::testing::Bool());

class SingleClientDictionaryTransportModeSyncTest
    : public SingleClientDictionarySyncTest {
 public:
  SingleClientDictionaryTransportModeSyncTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {// `kEnablePreferencesAccountStorage` and
         // `kSeparateLocalAndAccountSearchEngines`
         // are required for enabling dictionary sync in transport mode because
         // it shares the same user toggle as preferences and search engines.
         switches::kEnablePreferencesAccountStorage,
         syncer::kSeparateLocalAndAccountSearchEngines},
        /*disabled_features=*/{});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(SingleClientDictionaryTransportModeSyncTest,
                       ShouldStartDataTypeInTransportModeIfFeatureEnabled) {
  ASSERT_TRUE(SetupClients());

  // Sign in the primary account.
  ASSERT_TRUE(GetClient(0)->SignInPrimaryAccount());
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());

  // Whether or not the type is enabled in transport mode depends on the
  // `kSpellcheckSeparateLocalAndAccountDictionaries` feature flag.
  EXPECT_EQ(GetSyncService(0)->GetActiveDataTypes().Has(syncer::DICTIONARY),
            GetParam());
}

INSTANTIATE_TEST_SUITE_P(,
                         SingleClientDictionaryTransportModeSyncTest,
                         ::testing::Bool());

class SingleClientDictionaryWithAccountStorageSyncTest : public SyncTest {
 protected:
  SingleClientDictionaryWithAccountStorageSyncTest()
      : SyncTest(SINGLE_CLIENT) {}

  base::test::ScopedFeatureList feature_list_{
      syncer::kSpellcheckSeparateLocalAndAccountDictionaries};
};

IN_PROC_BROWSER_TEST_F(SingleClientDictionaryWithAccountStorageSyncTest,
                       ShouldNotUploadLocalWordsToTheAccount) {
  ASSERT_TRUE(SetupClients());
  dictionary_helper::LoadDictionaries();

  EXPECT_TRUE(dictionary_helper::GetDictionary(0)->AddWord(kLocalWord));
  EXPECT_THAT(dictionary_helper::GetDictionaryWords(0),
              UnorderedElementsAre(kLocalWord));

  // Enable Sync.
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::DICTIONARY));

  // No data is uploaded to the account.
  EXPECT_FALSE(
      dictionary_helper::HasWordInFakeServer(kLocalWord, GetFakeServer()));
  // Local words are still in the local dictionary.
  EXPECT_THAT(dictionary_helper::GetDictionaryWords(0),
              UnorderedElementsAre(kLocalWord));
}

IN_PROC_BROWSER_TEST_F(SingleClientDictionaryWithAccountStorageSyncTest,
                       ShouldCleanUpAccountWordsOnDisable) {
  ASSERT_TRUE(SetupClients());
  dictionary_helper::LoadDictionaries();

  EXPECT_TRUE(dictionary_helper::GetDictionary(0)->AddWord(kLocalWord));
  EXPECT_THAT(dictionary_helper::GetDictionaryWords(0),
              UnorderedElementsAre(kLocalWord));

  sync_pb::EntitySpecifics specifics;
  specifics.mutable_dictionary()->set_word(kAccountWord);
  GetFakeServer()->InjectEntity(
      syncer::PersistentUniqueClientEntity::CreateFromSpecificsForTesting(
          /*non_unique_name=*/kAccountWord,
          /*client_tag=*/kAccountWord, specifics,
          /*creation_time=*/0, /*last_modified_time=*/0));

  // Enable Sync.
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::DICTIONARY));

  EXPECT_THAT(dictionary_helper::GetDictionaryWords(0),
              UnorderedElementsAre(kLocalWord, kAccountWord));

  // Disable syncing dictionary, which is behind the preferences toggle.
  ASSERT_TRUE(GetClient(0)->DisableSyncForType(
      syncer::UserSelectableType::kPreferences));
  ASSERT_FALSE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::DICTIONARY));

  // Account words should be cleared.
  EXPECT_THAT(dictionary_helper::GetDictionaryWords(0),
              UnorderedElementsAre(kLocalWord));
  // No data is uploaded to the account.
  ASSERT_FALSE(
      dictionary_helper::HasWordInFakeServer(kLocalWord, GetFakeServer()));
  // ... but the account word is still there.
  ASSERT_TRUE(
      dictionary_helper::HasWordInFakeServer(kAccountWord, GetFakeServer()));
}

IN_PROC_BROWSER_TEST_F(SingleClientDictionaryWithAccountStorageSyncTest,
                       PRE_ShouldPersistAccountWordsOverRestarts) {
  ASSERT_TRUE(SetupClients());
  dictionary_helper::LoadDictionaries();

  EXPECT_TRUE(dictionary_helper::GetDictionary(0)->AddWord(kLocalWord));
  EXPECT_THAT(dictionary_helper::GetDictionaryWords(0),
              UnorderedElementsAre(kLocalWord));

  sync_pb::EntitySpecifics specifics;
  specifics.mutable_dictionary()->set_word(kAccountWord);
  GetFakeServer()->InjectEntity(
      syncer::PersistentUniqueClientEntity::CreateFromSpecificsForTesting(
          /*non_unique_name=*/kAccountWord,
          /*client_tag=*/kAccountWord, specifics,
          /*creation_time=*/0, /*last_modified_time=*/0));

  // Enable Sync.
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::DICTIONARY));

  ASSERT_THAT(dictionary_helper::GetDictionaryWords(0),
              UnorderedElementsAre(kLocalWord, kAccountWord));
}

IN_PROC_BROWSER_TEST_F(SingleClientDictionaryWithAccountStorageSyncTest,
                       ShouldPersistAccountWordsOverRestarts) {
  // Mimics network issues on restart.
  GetFakeServer()->SetHttpError(net::HTTP_REQUEST_TIMEOUT);

  ASSERT_TRUE(SetupClients());
  dictionary_helper::LoadDictionaries();

  // Wait for the account dictionary to be loaded from sync data. Account words
  // are loaded despite network issues, indicating that they're persisted.
  ASSERT_TRUE(dictionary_helper::NumDictionaryEntriesChecker(/*index=*/0,
                                                             /*num_words=*/2)
                  .Wait());

  // Account words should be present.
  EXPECT_THAT(dictionary_helper::GetDictionaryWords(0),
              UnorderedElementsAre(kLocalWord, kAccountWord));

  // Clear the error to allow sync to become active again.
  GetFakeServer()->ClearHttpError();
  // Disable syncing dictionary, which is behind the preferences toggle.
  ASSERT_TRUE(GetClient(0)->DisableSyncForType(
      syncer::UserSelectableType::kPreferences));
  ASSERT_FALSE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::DICTIONARY));

  // Account words should be cleared.
  EXPECT_THAT(dictionary_helper::GetDictionaryWords(0),
              UnorderedElementsAre(kLocalWord));
  // No data is uploaded to the account.
  ASSERT_FALSE(
      dictionary_helper::HasWordInFakeServer(kLocalWord, GetFakeServer()));
  // ... but the account word is still there.
  ASSERT_TRUE(
      dictionary_helper::HasWordInFakeServer(kAccountWord, GetFakeServer()));
}

}  // namespace
