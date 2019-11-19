// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/sync/test/integration/dictionary_helper.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/sync_integration_test_util.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/updated_progress_marker_checker.h"
#include "components/spellcheck/common/spellcheck_common.h"
#include "components/sync/base/model_type.h"

namespace {

using spellcheck::kMaxSyncableDictionaryWords;

class TwoClientDictionarySyncTest : public SyncTest {
 public:
  TwoClientDictionarySyncTest() : SyncTest(TWO_CLIENT) {}

  ~TwoClientDictionarySyncTest() override {}

  bool TestUsesSelfNotifications() override { return false; }

 private:
  DISALLOW_COPY_AND_ASSIGN(TwoClientDictionarySyncTest);
};

IN_PROC_BROWSER_TEST_F(TwoClientDictionarySyncTest, E2E_ENABLED(Sanity)) {
  ResetSyncForPrimaryAccount();
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  dictionary_helper::LoadDictionaries();
  ASSERT_TRUE(DictionaryMatchChecker().Wait());

  std::vector<std::string> words;
  words.push_back("foo");
  words.push_back("bar");
  ASSERT_EQ(num_clients(), static_cast<int>(words.size()));

  for (int i = 0; i < num_clients(); ++i) {
    ASSERT_TRUE(dictionary_helper::AddWord(i, words[i]));
  }
  ASSERT_TRUE(DictionaryMatchChecker().Wait());
  ASSERT_EQ(words.size(), dictionary_helper::GetDictionarySize(0));

  for (int i = 0; i < num_clients(); ++i) {
    ASSERT_TRUE(dictionary_helper::RemoveWord(i, words[i]));
  }
  ASSERT_TRUE(DictionaryMatchChecker().Wait());
  ASSERT_EQ(0UL, dictionary_helper::GetDictionarySize(0));

  DisableVerifier();
  for (int i = 0; i < num_clients(); ++i)
    ASSERT_TRUE(dictionary_helper::AddWord(i, words[i]));
  ASSERT_TRUE(DictionaryMatchChecker().Wait());
  ASSERT_EQ(words.size(), dictionary_helper::GetDictionarySize(0));
}

IN_PROC_BROWSER_TEST_F(TwoClientDictionarySyncTest,
                       E2E_ENABLED(SimultaneousAdd)) {
  ResetSyncForPrimaryAccount();
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  dictionary_helper::LoadDictionaries();
  ASSERT_TRUE(DictionaryMatchChecker().Wait());

  for (int i = 0; i < num_clients(); ++i)
    dictionary_helper::AddWord(i, "foo");
  ASSERT_TRUE(DictionaryMatchChecker().Wait());
  ASSERT_EQ(1UL, dictionary_helper::GetDictionarySize(0));
}

IN_PROC_BROWSER_TEST_F(TwoClientDictionarySyncTest,
                       E2E_ENABLED(SimultaneousRemove)) {
  ResetSyncForPrimaryAccount();
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  dictionary_helper::LoadDictionaries();
  ASSERT_TRUE(DictionaryMatchChecker().Wait());

  for (int i = 0; i < num_clients(); ++i)
    dictionary_helper::AddWord(i, "foo");
  ASSERT_TRUE(DictionaryMatchChecker().Wait());
  ASSERT_EQ(1UL, dictionary_helper::GetDictionarySize(0));

  for (int i = 0; i < num_clients(); ++i)
    dictionary_helper::RemoveWord(i, "foo");
  ASSERT_TRUE(DictionaryMatchChecker().Wait());
  ASSERT_EQ(0UL, dictionary_helper::GetDictionarySize(0));
}

IN_PROC_BROWSER_TEST_F(TwoClientDictionarySyncTest,
                       E2E_ENABLED(AddDifferentToEach)) {
  ResetSyncForPrimaryAccount();
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  dictionary_helper::LoadDictionaries();
  ASSERT_TRUE(DictionaryMatchChecker().Wait());

  for (int i = 0; i < num_clients(); ++i)
    dictionary_helper::AddWord(i, "foo" + base::NumberToString(i));

  ASSERT_TRUE(DictionaryMatchChecker().Wait());
  ASSERT_EQ(num_clients(),
            static_cast<int>(dictionary_helper::GetDictionarySize(0)));
}

IN_PROC_BROWSER_TEST_F(TwoClientDictionarySyncTest,
                       E2E_ENABLED(RemoveOnAAddOnB)) {
  ResetSyncForPrimaryAccount();
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  dictionary_helper::LoadDictionaries();
  ASSERT_TRUE(DictionaryMatchChecker().Wait());

  std::string word = "foo";
  // Add on client A, check it appears on B.
  ASSERT_TRUE(dictionary_helper::AddWord(0, word));
  ASSERT_TRUE(DictionaryMatchChecker().Wait());
  // Remove on client A, check it disappears on B.
  ASSERT_TRUE(dictionary_helper::RemoveWord(0, word));
  ASSERT_TRUE(DictionaryMatchChecker().Wait());
  // Add on client B, check it appears on A.
  ASSERT_TRUE(dictionary_helper::AddWord(1, word));
  ASSERT_TRUE(DictionaryMatchChecker().Wait());
  ASSERT_EQ(1UL, dictionary_helper::GetDictionarySize(0));
}

// Tests the case where a client has more words added than the
// kMaxSyncableDictionaryWords limit.
IN_PROC_BROWSER_TEST_F(TwoClientDictionarySyncTest, Limit) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  dictionary_helper::LoadDictionaries();
  ASSERT_TRUE(DictionaryMatchChecker().Wait());

  // Disable client #1 before client #0 starts adding anything.
  GetClient(1)->DisableSyncForAllDatatypes();

  // Pick a size between 1/2 and 1/3 of kMaxSyncableDictionaryWords. This will
  // allow the test to verify that while we crossed the limit the client not
  // actively making changes is still recieving sync updates but stops exactly
  // on the limit.
  size_t chunk_size = kMaxSyncableDictionaryWords * 2 / 5;

  ASSERT_TRUE(dictionary_helper::AddWords(0, chunk_size, "foo-0-"));
  ASSERT_EQ(chunk_size, dictionary_helper::GetDictionarySize(0));

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

  ASSERT_TRUE(dictionary_helper::AddWords(1, 2 * chunk_size, "foo-1-"));
  ASSERT_EQ(2 * chunk_size, dictionary_helper::GetDictionarySize(1));

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
