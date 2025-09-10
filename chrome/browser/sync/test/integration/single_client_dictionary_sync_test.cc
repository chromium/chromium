// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
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

}  // namespace
