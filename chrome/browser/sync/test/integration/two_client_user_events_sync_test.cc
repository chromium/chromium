// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/time/time.h"
#include "chrome/browser/sync/test/integration/bookmarks_helper.h"
#include "chrome/browser/sync/test/integration/encryption_helper.h"
#include "chrome/browser/sync/test/integration/sync_integration_test_util.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/updated_progress_marker_checker.h"
#include "chrome/browser/sync/test/integration/user_events_helper.h"
#include "chrome/browser/sync/user_event_service_factory.h"
#include "components/sync/protocol/user_event_specifics.pb.h"
#include "components/sync_user_events/user_event_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using bookmarks_helper::BookmarksMatchChecker;
using bookmarks_helper::CountBookmarksWithUrlsMatching;

const int kEncryptingClientId = 0;
const int kDecryptingClientId = 1;

const char kTestBookmarkURL[] = "https://google.com/synced-bookmark-1";

class TwoClientUserEventsSyncTest : public SyncTest {
 public:
  TwoClientUserEventsSyncTest() : SyncTest(TWO_CLIENT) {}

  ~TwoClientUserEventsSyncTest() override = default;

  bool ExpectNoUserEvent(int index) {
    return UserEventEqualityChecker(GetSyncService(index), GetFakeServer(),
                                    /*expected_specifics=*/{})
        .Wait();
  }

  void AddTestBookmarksToClient(int index) {
    ASSERT_TRUE(bookmarks_helper::AddURL(
        index, 0, "What are you syncing about?", GURL(kTestBookmarkURL)));
  }
};

IN_PROC_BROWSER_TEST_F(TwoClientUserEventsSyncTest,
                       SetPassphraseAndRecordEventAndThenSetupSync) {
  ASSERT_TRUE(SetupClients());
  ASSERT_TRUE(GetClient(kEncryptingClientId)->SetupSync());

  // Set up a sync client with custom passphrase to get the data encrypted on
  // the server.
  GetSyncService(kEncryptingClientId)
      ->GetUserSettings()
      ->SetEncryptionPassphrase("hunter2");
  UpdatedProgressMarkerChecker update_checker(
      GetSyncService(kEncryptingClientId));
  ASSERT_TRUE(
      PassphraseAcceptedChecker(GetSyncService(kEncryptingClientId)).Wait());
  // Make sure the updates are committed before proceeding with the test.
  ASSERT_TRUE(update_checker.Wait());

  // Record a user event on the second client before setting up sync (before
  // knowing it will be encrypted). This event should not get recorded while
  // starting up sync because the user has custom passphrase setup.
  syncer::UserEventService* event_service =
      browser_sync::UserEventServiceFactory::GetForProfile(GetProfile(0));
  event_service->RecordUserEvent(user_events_helper::CreateTestEvent(
      base::Time() + base::Microseconds(1)));

  // Set up sync on the second client.
  ASSERT_TRUE(GetClient(kDecryptingClientId)->SetupSyncNoWaitForCompletion());
  // The second client asks the user to provide a password for decryption.
  ASSERT_TRUE(
      PassphraseRequiredChecker(GetSyncService(kDecryptingClientId)).Wait());
  // Provide the password.
  ASSERT_TRUE(GetSyncService(kDecryptingClientId)
                  ->GetUserSettings()
                  ->SetDecryptionPassphrase("hunter2"));
  // Check it is accepted and finish the setup.
  ASSERT_TRUE(
      PassphraseAcceptedChecker(GetSyncService(kDecryptingClientId)).Wait());
  GetClient(kDecryptingClientId)->FinishSyncSetup();

  // Just checking that we don't see the recorded test event isn't very
  // convincing yet, because it may simply not have reached the server yet. So
  // let's send something else on the second client through the system that we
  // can wait on before checking. Bookmark data was picked fairly arbitrarily.
  AddTestBookmarksToClient(kDecryptingClientId);
  ASSERT_TRUE(BookmarksMatchChecker().Wait());

  // Double check that the encrypting client has the bookmark.
  ASSERT_EQ(1u, CountBookmarksWithUrlsMatching(kEncryptingClientId,
                                               GURL(kTestBookmarkURL)));

  // Finally, make sure no user event got sent to the server.
  EXPECT_TRUE(ExpectNoUserEvent(kDecryptingClientId));
}

}  // namespace
