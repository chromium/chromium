// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/sync/test/integration/dictionary_helper.h"
#include "chrome/browser/sync/test/integration/sync_integration_test_util.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/updated_progress_marker_checker.h"
#include "components/spellcheck/common/spellcheck_common.h"
#include "components/sync/base/data_type.h"
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

class TwoClientDictionarySyncTest : public SyncTest {
 public:
  TwoClientDictionarySyncTest() : SyncTest(TWO_CLIENT) {}
  ~TwoClientDictionarySyncTest() override = default;

  bool TestUsesSelfNotifications() override { return false; }
};

IN_PROC_BROWSER_TEST_F(TwoClientDictionarySyncTest, E2E_ENABLED(Sanity)) {
  ResetSyncForPrimaryAccount();
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
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

IN_PROC_BROWSER_TEST_F(TwoClientDictionarySyncTest,
                       E2E_ENABLED(SimultaneousAdd)) {
  ResetSyncForPrimaryAccount();
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  LoadDictionaries();
  ASSERT_TRUE(DictionaryChecker(/*expected_words=*/{}).Wait());

  const std::string word = "foo";
  for (int i = 0; i < num_clients(); ++i) {
    dictionary_helper::AddWord(i, word);
  }
  EXPECT_TRUE(DictionaryChecker(/*expected_words=*/{word}).Wait());
}

IN_PROC_BROWSER_TEST_F(TwoClientDictionarySyncTest,
                       E2E_ENABLED(SimultaneousRemove)) {
  ResetSyncForPrimaryAccount();
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
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

IN_PROC_BROWSER_TEST_F(TwoClientDictionarySyncTest,
                       E2E_ENABLED(RemoveOnAAddOnB)) {
  ResetSyncForPrimaryAccount();
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
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
IN_PROC_BROWSER_TEST_F(TwoClientDictionarySyncTest, Limit) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  LoadDictionaries();
  ASSERT_TRUE(DictionaryChecker(/*expected_words=*/{}).Wait());

  // Disable client #1 before client #0 starts adding anything.
  ASSERT_TRUE(GetClient(1)->DisableSyncForAllDatatypes());

  // Pick a size between 1/2 and 1/3 of kMaxSyncableDictionaryWords. This will
  // allow the test to verify that while we crossed the limit the client not
  // actively making changes is still recieving sync updates but stops exactly
  // on the limit.
  size_t chunk_size = kMaxSyncableDictionaryWords * 2 / 5;

  ASSERT_TRUE(AddWords(0, chunk_size, "foo-0-"));
  ASSERT_EQ(chunk_size, GetDictionarySize(0));

  // We must wait for the server here. This test was originally an n-client test
  // where n-1 clients waited to have the same state. We cannot do that on 2
  // clients because one needs to be disconnected during this process, because
  // part of what we're testing is that when it comes online it pulls remote
  // changes before pushing local changes, with the limit in mind. So we check
  // the server count here to make sure client #0 is done pushing its changes
  // out. In there real world there's a race condition here, if multiple clients
  // are adding words simultaneously then we're go over the limit slightly,
  // though we'd expect this to be relatively small.
  ASSERT_TRUE(
      ServerCountMatchStatusChecker(syncer::DICTIONARY, chunk_size).Wait());

  ASSERT_TRUE(AddWords(1, 2 * chunk_size, "foo-1-"));
  ASSERT_EQ(2 * chunk_size, GetDictionarySize(1));

  // Client #1 should first pull remote changes, apply them, without capping at
  // any sort of limit. This will cause client #1 to have 3 * chunk_size. When
  // client #1 then tries to commit changes, that is when it obeys the limit
  // and will cause client #0 to only see the limit worth of words.
  ASSERT_TRUE(GetClient(1)->EnableSyncForRegisteredDatatypes());
  ASSERT_TRUE(NumDictionaryEntriesChecker(1, 3 * chunk_size).Wait());
  ASSERT_TRUE(ServerCountMatchStatusChecker(syncer::DICTIONARY,
                                            kMaxSyncableDictionaryWords)
                  .Wait());
  ASSERT_TRUE(
      NumDictionaryEntriesChecker(0, kMaxSyncableDictionaryWords).Wait());
}

}  // namespace
