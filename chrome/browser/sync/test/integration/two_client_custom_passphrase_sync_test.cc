// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/bookmarks_helper.h"
#include "chrome/browser/sync/test/integration/encryption_helper.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/sync/base/sync_base_switches.h"
#include "components/sync/engine/sync_engine_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

static const int kEncryptingClientId = 0;
static const int kDecryptingClientId = 1;

using bookmarks_helper::AddURL;
using bookmarks_helper::AllModelsMatchVerifier;

// These tests consider the client as a black-box; they are not concerned with
// whether the data is committed to the server correctly encrypted. Rather, they
// test the end-to-end behavior of two clients when a custom passphrase is set,
// i.e. whether the second client can see data that was committed by the first
// client. To test proper encryption behavior, a separate single-client test is
// used.
class TwoClientCustomPassphraseSyncTest : public SyncTest {
 public:
  TwoClientCustomPassphraseSyncTest() : SyncTest(TWO_CLIENT) {}
  ~TwoClientCustomPassphraseSyncTest() override {}

  bool WaitForBookmarksToMatchVerifier() {
    return BookmarksMatchVerifierChecker().Wait();
  }

  bool WaitForPassphraseRequiredState(int index, bool desired_state) {
    return PassphraseRequiredStateChecker(GetSyncService(index), desired_state)
        .Wait();
  }

  void AddTestBookmarksToClient(int index) {
    ASSERT_TRUE(AddURL(index, 0, "What are you syncing about?",
                       GURL("https://google.com/synced-bookmark-1")));
    ASSERT_TRUE(AddURL(index, 1, "Test bookmark",
                       GURL("https://google.com/synced-bookmark-2")));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TwoClientCustomPassphraseSyncTest);
};

IN_PROC_BROWSER_TEST_F(TwoClientCustomPassphraseSyncTest,
                       DecryptionFailsWhenIncorrectPassphraseProvided) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  GetSyncService(kEncryptingClientId)->SetEncryptionPassphrase("hunter2");
  ASSERT_TRUE(WaitForPassphraseRequiredState(kDecryptingClientId,
                                             /*desired_state=*/true));
  EXPECT_FALSE(GetSyncService(kDecryptingClientId)
                   ->SetDecryptionPassphrase("incorrect passphrase"));
  EXPECT_TRUE(
      GetSyncService(kDecryptingClientId)->IsPassphraseRequiredForDecryption());
}

IN_PROC_BROWSER_TEST_F(TwoClientCustomPassphraseSyncTest, ClientsCanSyncData) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  GetSyncService(kEncryptingClientId)->SetEncryptionPassphrase("hunter2");
  ASSERT_TRUE(WaitForPassphraseRequiredState(kDecryptingClientId,
                                             /*desired_state=*/true));
  EXPECT_TRUE(
      GetSyncService(kDecryptingClientId)->SetDecryptionPassphrase("hunter2"));
  EXPECT_TRUE(WaitForPassphraseRequiredState(kDecryptingClientId,
                                             /*desired_state=*/false));
  AddTestBookmarksToClient(kEncryptingClientId);

  ASSERT_TRUE(
      GetClient(kEncryptingClientId)
          ->AwaitMutualSyncCycleCompletion(GetClient(kDecryptingClientId)));
  EXPECT_TRUE(WaitForBookmarksToMatchVerifier());
}

IN_PROC_BROWSER_TEST_F(TwoClientCustomPassphraseSyncTest,
                       ClientsCanSyncDataWhenScryptEncryptionNotEnabled) {
  ScopedScryptFeatureToggler toggler(/*force_disabled=*/false,
                                     /*use_for_new_passphrases=*/false);
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  GetSyncService(kEncryptingClientId)->SetEncryptionPassphrase("hunter2");
  ASSERT_TRUE(WaitForPassphraseRequiredState(kDecryptingClientId,
                                             /*desired_state=*/true));
  EXPECT_TRUE(
      GetSyncService(kDecryptingClientId)->SetDecryptionPassphrase("hunter2"));
  EXPECT_TRUE(WaitForPassphraseRequiredState(kDecryptingClientId,
                                             /*desired_state=*/false));
  AddTestBookmarksToClient(kEncryptingClientId);

  ASSERT_TRUE(
      GetClient(kEncryptingClientId)
          ->AwaitMutualSyncCycleCompletion(GetClient(kDecryptingClientId)));
  EXPECT_TRUE(WaitForBookmarksToMatchVerifier());
}

IN_PROC_BROWSER_TEST_F(TwoClientCustomPassphraseSyncTest,
                       ClientsCanSyncDataWhenScryptEncryptionEnabledInBoth) {
  ScopedScryptFeatureToggler toggler(/*force_disabled=*/false,
                                     /*use_for_new_passphrases=*/true);
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  GetSyncService(kEncryptingClientId)->SetEncryptionPassphrase("hunter2");
  ASSERT_TRUE(WaitForPassphraseRequiredState(kDecryptingClientId,
                                             /*desired_state=*/true));
  EXPECT_TRUE(
      GetSyncService(kDecryptingClientId)->SetDecryptionPassphrase("hunter2"));
  EXPECT_TRUE(WaitForPassphraseRequiredState(kDecryptingClientId,
                                             /*desired_state=*/false));
  AddTestBookmarksToClient(kEncryptingClientId);

  ASSERT_TRUE(
      GetClient(kEncryptingClientId)
          ->AwaitMutualSyncCycleCompletion(GetClient(kDecryptingClientId)));
  EXPECT_TRUE(WaitForBookmarksToMatchVerifier());
}

IN_PROC_BROWSER_TEST_F(TwoClientCustomPassphraseSyncTest,
                       ClientsCanSyncDataWhenScryptEncryptionEnabledInOne) {
  ScopedScryptFeatureToggler toggler(/*force_disabled=*/false,
                                     /*use_for_new_passphrases=*/false);
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  {
    ScopedScryptFeatureToggler temporary_toggler(
        /*force_disabled=*/false, /*use_for_new_passphrases=*/true);
    GetSyncService(kEncryptingClientId)->SetEncryptionPassphrase("hunter2");
  }
  ASSERT_TRUE(WaitForPassphraseRequiredState(kDecryptingClientId,
                                             /*desired_state=*/true));
  EXPECT_TRUE(
      GetSyncService(kDecryptingClientId)->SetDecryptionPassphrase("hunter2"));
  EXPECT_TRUE(WaitForPassphraseRequiredState(kDecryptingClientId,
                                             /*desired_state=*/false));
  AddTestBookmarksToClient(kEncryptingClientId);

  ASSERT_TRUE(
      GetClient(kEncryptingClientId)
          ->AwaitMutualSyncCycleCompletion(GetClient(kDecryptingClientId)));
  EXPECT_TRUE(WaitForBookmarksToMatchVerifier());
}

}  // namespace
