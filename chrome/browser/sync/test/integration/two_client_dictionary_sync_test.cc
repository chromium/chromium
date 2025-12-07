// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/sync/test/integration/dictionary_helper.h"
#include "chrome/browser/sync/test/integration/sync_integration_test_util.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/updated_progress_marker_checker.h"
#include "components/spellcheck/common/spellcheck_common.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/features.h"
#include "content/public/test/browser_test.h"

namespace {

using dictionary_helper::AddWord;
using dictionary_helper::AddWords;
using dictionary_helper::DictionaryChecker;
using dictionary_helper::GetDictionarySize;
using dictionary_helper::LoadDictionaries;
using dictionary_helper::NumDictionaryEntriesChecker;
using dictionary_helper::RemoveWord;
using spellcheck::kMaxSyncableDictionaryWords;

class TwoClientDictionarySyncTest
    : public SyncTest,
      public testing::WithParamInterface<SyncTest::SetupSyncMode> {
 public:
  TwoClientDictionarySyncTest() : SyncTest(TWO_CLIENT) {
    if (GetSetupSyncMode() == SetupSyncMode::kSyncTransportOnly) {
      scoped_feature_list_.InitWithFeatures(
          {syncer::kReplaceSyncPromosWithSignInPromos,
           syncer::kSpellcheckSeparateLocalAndAccountDictionaries},
          {});
    }
  }

  ~TwoClientDictionarySyncTest() override = default;

  SyncTest::SetupSyncMode GetSetupSyncMode() const override {
    return GetParam();
  }

  bool TestUsesSelfNotifications() override { return false; }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    TwoClientDictionarySyncTest,
    GetSyncTestModes(),
    testing::PrintToStringParamName());

IN_PROC_BROWSER_TEST_P(TwoClientDictionarySyncTest, E2E_ENABLED(Sanity)) {
  ASSERT_TRUE(ResetSyncForPrimaryAccount());
  ASSERT_TRUE(SetupSync());
  LoadDictionaries();
  EXPECT_TRUE(DictionaryChecker(/*expected_words=*/{}).Wait());

  const std::vector<std::string> words{"foo", "bar"};
  ASSERT_EQ(num_clients(), static_cast<int>(words.size()));

  for (int i = 0; i < num_clients(); ++i) {
    EXPECT_TRUE(AddWord(i, words[i]));
  }
  EXPECT_TRUE(DictionaryChecker(words).Wait());

  for (int i = 0; i < num_clients(); ++i) {
    EXPECT_TRUE(RemoveWord(i, words[i]));
  }
  EXPECT_TRUE(DictionaryChecker(/*expected_words=*/{}).Wait());

  for (int i = 0; i < num_clients(); ++i) {
    EXPECT_TRUE(AddWord(i, words[i]));
  }
  EXPECT_TRUE(DictionaryChecker(words).Wait());
}

IN_PROC_BROWSER_TEST_P(TwoClientDictionarySyncTest,
                       E2E_ENABLED(SimultaneousAdd)) {
  ASSERT_TRUE(ResetSyncForPrimaryAccount());
  ASSERT_TRUE(SetupSync());
  LoadDictionaries();
  ASSERT_TRUE(DictionaryChecker(/*expected_words=*/{}).Wait());

  const std::string word = "foo";
  for (int i = 0; i < num_clients(); ++i) {
    dictionary_helper::AddWord(i, word);
  }
  EXPECT_TRUE(DictionaryChecker(/*expected_words=*/{word}).Wait());
}

IN_PROC_BROWSER_TEST_P(TwoClientDictionarySyncTest,
                       E2E_ENABLED(SimultaneousRemove)) {
  ASSERT_TRUE(ResetSyncForPrimaryAccount());
  ASSERT_TRUE(SetupSync());
  LoadDictionaries();
  ASSERT_TRUE(DictionaryChecker(/*expected_words=*/{}).Wait());

  const std::string word = "foo";
  for (int i = 0; i < num_clients(); ++i) {
    AddWord(i, word);
  }
  ASSERT_TRUE(DictionaryChecker(/*expected_words=*/{word}).Wait());

  for (int i = 0; i < num_clients(); ++i) {
    RemoveWord(i, word);
  }
  EXPECT_TRUE(DictionaryChecker(/*expected_words=*/{}).Wait());
}

IN_PROC_BROWSER_TEST_P(TwoClientDictionarySyncTest,
                       E2E_ENABLED(RemoveOnAAddOnB)) {
  ASSERT_TRUE(ResetSyncForPrimaryAccount());
  ASSERT_TRUE(SetupSync());
  LoadDictionaries();
  ASSERT_TRUE(DictionaryChecker(/*expected_words=*/{}).Wait());

  const std::string word = "foo";
  // Add on client A, check it appears on B.
  ASSERT_TRUE(AddWord(0, word));
  EXPECT_TRUE(DictionaryChecker(/*expected_words=*/{word}).Wait());
  // Remove on client A, check it disappears on B.
  EXPECT_TRUE(RemoveWord(0, word));
  EXPECT_TRUE(DictionaryChecker(/*expected_words=*/{}).Wait());
  // Add on client B, check it appears on A.
  EXPECT_TRUE(AddWord(1, word));
  EXPECT_TRUE(DictionaryChecker(/*expected_words=*/{word}).Wait());
}

// Tests the case where a client has more words added than the
// kMaxSyncableDictionaryWords limit.
IN_PROC_BROWSER_TEST_P(TwoClientDictionarySyncTest, Limit) {
  ASSERT_TRUE(SetupSync());
  LoadDictionaries();
  ASSERT_TRUE(DictionaryChecker(/*expected_words=*/{}).Wait());

  // Pick a size between 1/2 and 1/3 of kMaxSyncableDictionaryWords. This will
  // allow the test to verify that while we crossed the limit the client not
  // actively making changes is still recieving sync updates but stops exactly
  // on the limit.
  const size_t chunk_size = kMaxSyncableDictionaryWords * 2 / 5;

  // Add `chunk_size` words to client #0 and wait for client #1 to receive them.
  ASSERT_TRUE(AddWords(0, chunk_size, "foo-0-"));
  ASSERT_EQ(chunk_size, GetDictionarySize(0));

  ASSERT_TRUE(
      ServerCountMatchStatusChecker(syncer::DICTIONARY, chunk_size).Wait());
  ASSERT_TRUE(NumDictionaryEntriesChecker(1, chunk_size).Wait());

  // Now add 2x `chunk_size` words to client #1, this will cause the count of
  // words on client #1 to exceed the limit. Out of which only words up till the
  // limit will be uploaded to the server.
  ASSERT_TRUE(AddWords(1, 2 * chunk_size, "foo-1-"));
  ASSERT_EQ(3 * chunk_size, GetDictionarySize(1));

  ASSERT_TRUE(ServerCountMatchStatusChecker(syncer::DICTIONARY,
                                            kMaxSyncableDictionaryWords)
                  .Wait());
  ASSERT_TRUE(
      NumDictionaryEntriesChecker(0, kMaxSyncableDictionaryWords).Wait());
}

}  // namespace
