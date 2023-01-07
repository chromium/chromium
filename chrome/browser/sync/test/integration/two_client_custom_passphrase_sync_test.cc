// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/sync/test/integration/bookmarks_helper.h"
#include "chrome/browser/sync/test/integration/encryption_helper.h"
#include "chrome/browser/sync/test/integration/sync_integration_test_util.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/updated_progress_marker_checker.h"
#include "components/sync/test/nigori_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_launcher.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace {

static const int kEncryptingClientId = 0;
static const int kDecryptingClientId = 1;

using bookmarks_helper::AddURL;
using bookmarks_helper::AllModelsMatch;
using bookmarks_helper::BookmarksMatchChecker;

// These tests consider the client as a black-box; they are not concerned with
// whether the data is committed to the server correctly encrypted. Rather, they
// test the end-to-end behavior of two clients when a custom passphrase is set,
// i.e. whether the second client can see data that was committed by the first
// client. To test proper encryption behavior, a separate single-client test is
// used.
class TwoClientCustomPassphraseSyncTest : public SyncTest {
 public:
  TwoClientCustomPassphraseSyncTest() : SyncTest(TWO_CLIENT) {}
  ~TwoClientCustomPassphraseSyncTest() override = default;

  bool WaitForBookmarksToMatch() { return BookmarksMatchChecker().Wait(); }

  bool WaitForPassphraseRequired(int index) {
    return PassphraseRequiredChecker(GetSyncService(index)).Wait();
  }

  bool WaitForPassphraseAccepted(int index) {
    return PassphraseAcceptedChecker(GetSyncService(index)).Wait();
  }

  void AddTestBookmarksToClient(int index) {
    ASSERT_TRUE(AddURL(index, 0, "What are you syncing about?",
                       GURL("https://google.com/synced-bookmark-1")));
    ASSERT_TRUE(AddURL(index, 1, "Test bookmark",
                       GURL("https://google.com/synced-bookmark-2")));
  }
};

IN_PROC_BROWSER_TEST_F(TwoClientCustomPassphraseSyncTest,
                       DecryptionFailsWhenIncorrectPassphraseProvided) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatch());

  GetSyncService(kEncryptingClientId)
      ->GetUserSettings()
      ->SetEncryptionPassphrase("hunter2");
  ASSERT_TRUE(WaitForPassphraseRequired(kDecryptingClientId));
  EXPECT_FALSE(GetSyncService(kDecryptingClientId)
                   ->GetUserSettings()
                   ->SetDecryptionPassphrase("incorrect passphrase"));
  EXPECT_TRUE(GetSyncService(kDecryptingClientId)
                  ->GetUserSettings()
                  ->IsPassphraseRequiredForPreferredDataTypes());
}

IN_PROC_BROWSER_TEST_F(TwoClientCustomPassphraseSyncTest, ClientsCanSyncData) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatch());

  GetSyncService(kEncryptingClientId)
      ->GetUserSettings()
      ->SetEncryptionPassphrase("hunter2");
  ASSERT_TRUE(WaitForPassphraseRequired(kDecryptingClientId));
  EXPECT_TRUE(GetSyncService(kDecryptingClientId)
                  ->GetUserSettings()
                  ->SetDecryptionPassphrase("hunter2"));
  EXPECT_TRUE(WaitForPassphraseAccepted(kDecryptingClientId));
  AddTestBookmarksToClient(kEncryptingClientId);

  ASSERT_TRUE(
      GetClient(kEncryptingClientId)
          ->AwaitMutualSyncCycleCompletion(GetClient(kDecryptingClientId)));
  EXPECT_TRUE(WaitForBookmarksToMatch());
}

IN_PROC_BROWSER_TEST_F(TwoClientCustomPassphraseSyncTest,
                       SetPassphraseAndThenSetupSync) {
  ASSERT_TRUE(SetupClients());
  ASSERT_TRUE(GetClient(kEncryptingClientId)->SetupSync());

  // Set up a sync client with custom passphrase and one bookmark.
  GetSyncService(kEncryptingClientId)
      ->GetUserSettings()
      ->SetEncryptionPassphrase("hunter2");
  ASSERT_TRUE(
      PassphraseAcceptedChecker(GetSyncService(kEncryptingClientId)).Wait());
  AddTestBookmarksToClient(kEncryptingClientId);
  // Wait for the client to commit the update.
  ASSERT_TRUE(
      UpdatedProgressMarkerChecker(GetSyncService(kEncryptingClientId)).Wait());

  // Set up a new sync client.
  ASSERT_TRUE(GetClient(kDecryptingClientId)->SetupSyncNoWaitForCompletion());
  ASSERT_TRUE(
      PassphraseRequiredChecker(GetSyncService(kDecryptingClientId)).Wait());

  // Get client |kDecryptingClientId| out of the passphrase required state.
  ASSERT_TRUE(GetSyncService(kDecryptingClientId)
                  ->GetUserSettings()
                  ->SetDecryptionPassphrase("hunter2"));
  ASSERT_TRUE(
      PassphraseAcceptedChecker(GetSyncService(kDecryptingClientId)).Wait());

  // Double check that bookmark models are not synced.
  ASSERT_FALSE(AllModelsMatch());
  GetClient(kDecryptingClientId)->FinishSyncSetup();

  // Wait for bookmarks to converge.
  EXPECT_TRUE(WaitForBookmarksToMatch());
}

}  // namespace
