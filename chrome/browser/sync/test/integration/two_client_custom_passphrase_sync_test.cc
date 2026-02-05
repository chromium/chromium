// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/browser/sync/test/integration/bookmarks_helper.h"
#include "chrome/browser/sync/test/integration/encryption_helper.h"
#include "chrome/browser/sync/test/integration/sync_integration_test_util.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/updated_progress_marker_checker.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/sync/test/nigori_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_launcher.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/constants/ash_features.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace {

static const int kEncryptingClientId = 0;
static const int kDecryptingClientId = 1;

using bookmarks_helper::AddURL;
using bookmarks_helper::AllModelsMatch;
using bookmarks_helper::BookmarksMatchChecker;
using bookmarks_helper::StoreType;

// These tests consider the client as a black-box; they are not concerned with
// whether the data is committed to the server correctly encrypted. Rather, they
// test the end-to-end behavior of two clients when a custom passphrase is set,
// i.e. whether the second client can see data that was committed by the first
// client. To test proper encryption behavior, a separate single-client test is
// used.
class TwoClientCustomPassphraseSyncTest
    : public SyncTest,
      public testing::WithParamInterface<SyncTest::SetupSyncMode> {
 public:
  TwoClientCustomPassphraseSyncTest() : SyncTest(TWO_CLIENT) {
    if (GetSetupSyncMode() == SetupSyncMode::kSyncTransportOnly) {
      scoped_feature_list_.InitAndEnableFeature(
          syncer::kReplaceSyncPromosWithSignInPromos);
    }
  }
  ~TwoClientCustomPassphraseSyncTest() override = default;

  SyncTest::SetupSyncMode GetSetupSyncMode() const override {
    return GetParam();
  }

  bool WaitForBookmarksToMatch() { return BookmarksMatchChecker().Wait(); }

  bool WaitForPassphraseRequired(int index) {
    return PassphraseRequiredChecker(GetSyncService(index)).Wait();
  }

  bool WaitForPassphraseAccepted(int index) {
    return PassphraseAcceptedChecker(GetSyncService(index)).Wait();
  }

  void AddTestBookmarksToClient(int index) {
    StoreType store_type =
        GetSetupSyncMode() == SyncTest::SetupSyncMode::kSyncTransportOnly
            ? StoreType::kAccountStore
            : StoreType::kLocalOrSyncableStore;
    ASSERT_TRUE(AddURL(index, 0, u"What are you syncing about?",
                       GURL("https://google.com/synced-bookmark-1"),
                       store_type));
    ASSERT_TRUE(AddURL(index, 1, u"Test bookmark",
                       GURL("https://google.com/synced-bookmark-2"),
                       store_type));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(,
                         TwoClientCustomPassphraseSyncTest,
                         GetSyncTestModes(),
                         testing::PrintToStringParamName());

IN_PROC_BROWSER_TEST_P(TwoClientCustomPassphraseSyncTest,
                       DecryptionFailsWhenIncorrectPassphraseProvided) {
  ASSERT_TRUE(SetupSync());
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

IN_PROC_BROWSER_TEST_P(TwoClientCustomPassphraseSyncTest, ClientsCanSyncData) {
  ASSERT_TRUE(SetupSync());
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

IN_PROC_BROWSER_TEST_P(TwoClientCustomPassphraseSyncTest,
                       SetPassphraseAndThenSetupSync) {
  ASSERT_TRUE(SetupClients());
  if (GetSetupSyncMode() == SetupSyncMode::kSyncTheFeature) {
    ASSERT_TRUE(GetClient(kEncryptingClientId)->SetupSync());
  } else {
    ASSERT_TRUE(GetClient(kEncryptingClientId)->SignInPrimaryAccount());
    ASSERT_TRUE(GetClient(kEncryptingClientId)->AwaitSyncTransportActive());
  }

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

  // Set up the second (decrypting) sync client.
  if (GetSetupSyncMode() == SetupSyncMode::kSyncTheFeature) {
    ASSERT_TRUE(GetClient(kDecryptingClientId)->SetupSyncNoWaitForCompletion());
  } else {
    ASSERT_TRUE(GetClient(kDecryptingClientId)->SignInPrimaryAccount());
  }
  ASSERT_TRUE(
      PassphraseRequiredChecker(GetSyncService(kDecryptingClientId)).Wait());

  // Get client `kDecryptingClientId` out of the passphrase required state.
  ASSERT_TRUE(GetSyncService(kDecryptingClientId)
                  ->GetUserSettings()
                  ->SetDecryptionPassphrase("hunter2"));
  ASSERT_TRUE(
      PassphraseAcceptedChecker(GetSyncService(kDecryptingClientId)).Wait());

  if (GetSetupSyncMode() == SetupSyncMode::kSyncTheFeature) {
    // Double check that bookmark models are not synced.
    ASSERT_FALSE(AllModelsMatch());
    GetClient(kDecryptingClientId)->FinishSyncSetup();
  }

  // Wait for bookmarks to converge.
  EXPECT_TRUE(WaitForBookmarksToMatch());
}

}  // namespace
