// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/macros.h"
#include "base/test/bind_test_util.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/sync/test/integration/bookmarks_helper.h"
#include "chrome/browser/sync/test/integration/encryption_helper.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/sessions_helper.h"
#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"
#include "chrome/browser/sync/test/integration/status_change_checker.h"
#include "chrome/browser/sync/test/integration/sync_integration_test_util.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/user_events_helper.h"
#include "chrome/browser/sync/user_event_service_factory.h"
#include "components/sync/protocol/user_event_specifics.pb.h"
#include "components/sync_user_events/user_event_service.h"

using sync_pb::CommitResponse;
using sync_pb::SyncEntity;
using sync_pb::UserEventSpecifics;
using user_events_helper::CreateTestEvent;

namespace {

CommitResponse::ResponseType BounceType(
    CommitResponse::ResponseType type,
    const syncer::LoopbackServerEntity& entity) {
  return type;
}

class SingleClientUserEventsSyncTest : public SyncTest {
 public:
  SingleClientUserEventsSyncTest() : SyncTest(SINGLE_CLIENT) {
    DisableVerifier();
  }

  bool ExpectUserEvents(std::vector<UserEventSpecifics> expected_specifics) {
    return UserEventEqualityChecker(GetSyncService(0), GetFakeServer(),
                                    expected_specifics)
        .Wait();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SingleClientUserEventsSyncTest);
};

IN_PROC_BROWSER_TEST_F(SingleClientUserEventsSyncTest, Sanity) {
  ASSERT_TRUE(SetupSync());
  EXPECT_EQ(
      0u,
      GetFakeServer()->GetSyncEntitiesByModelType(syncer::USER_EVENTS).size());
  syncer::UserEventService* event_service =
      browser_sync::UserEventServiceFactory::GetForProfile(GetProfile(0));
  const UserEventSpecifics specifics = CreateTestEvent(base::Time());
  event_service->RecordUserEvent(specifics);
  EXPECT_TRUE(ExpectUserEvents({specifics}));
}

IN_PROC_BROWSER_TEST_F(SingleClientUserEventsSyncTest, RetrySequential) {
  ASSERT_TRUE(SetupSync());
  const UserEventSpecifics specifics1 =
      CreateTestEvent(base::Time() + base::TimeDelta::FromMicroseconds(1));
  const UserEventSpecifics specifics2 =
      CreateTestEvent(base::Time() + base::TimeDelta::FromMicroseconds(2));
  syncer::UserEventService* event_service =
      browser_sync::UserEventServiceFactory::GetForProfile(GetProfile(0));

  GetFakeServer()->OverrideResponseType(
      base::Bind(&BounceType, CommitResponse::TRANSIENT_ERROR));
  event_service->RecordUserEvent(specifics1);

  // This will block until we hit a TRANSIENT_ERROR, at which point we will
  // regain control and can switch back to SUCCESS.
  EXPECT_TRUE(ExpectUserEvents({specifics1}));
  GetFakeServer()->OverrideResponseType(
      base::Bind(&BounceType, CommitResponse::SUCCESS));
  // Because the fake server records commits even on failure, we are able to
  // verify that the commit for this event reached the server twice.
  EXPECT_TRUE(ExpectUserEvents({specifics1, specifics1}));

  // Only record |specifics2| after |specifics1| was successful to avoid race
  // conditions.
  event_service->RecordUserEvent(specifics2);
  EXPECT_TRUE(ExpectUserEvents({specifics1, specifics1, specifics2}));
}

// Flaky (mostly) on ASan/TSan. http://crbug.com/998130
#if defined(ADDRESS_SANITIZER) || defined(THREAD_SANITIZER)
#define MAYBE_RetryParallel DISABLED_RetryParallel
#else
#define MAYBE_RetryParallel RetryParallel
#endif
IN_PROC_BROWSER_TEST_F(SingleClientUserEventsSyncTest, MAYBE_RetryParallel) {
  ASSERT_TRUE(SetupSync());

  const UserEventSpecifics specifics1 =
      CreateTestEvent(base::Time() + base::TimeDelta::FromMicroseconds(1));
  const UserEventSpecifics specifics2 =
      CreateTestEvent(base::Time() + base::TimeDelta::FromMicroseconds(2));

  syncer::UserEventService* event_service =
      browser_sync::UserEventServiceFactory::GetForProfile(GetProfile(0));

  // Set up the server so that the first entity that arrives results in a
  // transient error.
  // We're not really sure if |specifics1| or |specifics2| is going to see the
  // error, so record the one that does into |retry_specifics| and use it in
  // expectations.
  bool first = true;
  UserEventSpecifics retry_specifics;
  GetFakeServer()->OverrideResponseType(base::BindLambdaForTesting(
      [&](const syncer::LoopbackServerEntity& entity) {
        if (first) {
          first = false;
          SyncEntity sync_entity;
          entity.SerializeAsProto(&sync_entity);
          retry_specifics = sync_entity.specifics().user_event();
          return CommitResponse::TRANSIENT_ERROR;
        }
        return CommitResponse::SUCCESS;
      }));

  event_service->RecordUserEvent(specifics2);
  event_service->RecordUserEvent(specifics1);
  // First wait for these two events to arrive on the server - only after this
  // has happened will |retry_specifics| actually be populated.
  // Note: The entity that got the transient error is still considered
  // "committed" by the fake server.
  EXPECT_TRUE(ExpectUserEvents({specifics1, specifics2}));
  // Now that |retry_specifics| got populated by the lambda above, make sure it
  // also arrives on the server.
  EXPECT_TRUE(ExpectUserEvents({specifics1, specifics2, retry_specifics}));
}

IN_PROC_BROWSER_TEST_F(SingleClientUserEventsSyncTest, NoHistory) {
  const UserEventSpecifics test_event1 =
      CreateTestEvent(base::Time() + base::TimeDelta::FromMicroseconds(1));
  const UserEventSpecifics test_event2 =
      CreateTestEvent(base::Time() + base::TimeDelta::FromMicroseconds(2));
  const UserEventSpecifics test_event3 =
      CreateTestEvent(base::Time() + base::TimeDelta::FromMicroseconds(3));

  ASSERT_TRUE(SetupSync());
  syncer::UserEventService* event_service =
      browser_sync::UserEventServiceFactory::GetForProfile(GetProfile(0));

  event_service->RecordUserEvent(test_event1);

  // Wait until the first events is committed before disabling sync,
  // because disabled kHistory also disables user event sync, dropping all
  // uncommitted events.
  EXPECT_TRUE(ExpectUserEvents({test_event1}));
  ASSERT_TRUE(
      GetClient(0)->DisableSyncForType(syncer::UserSelectableType::kHistory));

  event_service->RecordUserEvent(test_event2);
  ASSERT_TRUE(
      GetClient(0)->EnableSyncForType(syncer::UserSelectableType::kHistory));
  event_service->RecordUserEvent(test_event3);

  // No |test_event2| because it was recorded while history was disabled.
  EXPECT_TRUE(ExpectUserEvents({test_event1, test_event3}));
}

IN_PROC_BROWSER_TEST_F(SingleClientUserEventsSyncTest, NoSessions) {
  const UserEventSpecifics specifics =
      CreateTestEvent(base::Time() + base::TimeDelta::FromMicroseconds(1));
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(
      GetClient(0)->DisableSyncForType(syncer::UserSelectableType::kTabs));
  syncer::UserEventService* event_service =
      browser_sync::UserEventServiceFactory::GetForProfile(GetProfile(0));

  event_service->RecordUserEvent(specifics);

  // PROXY_TABS shouldn't affect us in any way.
  EXPECT_TRUE(ExpectUserEvents({specifics}));
}

IN_PROC_BROWSER_TEST_F(SingleClientUserEventsSyncTest, Encryption) {
  const UserEventSpecifics test_event1 =
      CreateTestEvent(base::Time() + base::TimeDelta::FromMicroseconds(1));
  const UserEventSpecifics test_event2 =
      CreateTestEvent(base::Time() + base::TimeDelta::FromMicroseconds(2));

  ASSERT_TRUE(SetupSync());
  syncer::UserEventService* event_service =
      browser_sync::UserEventServiceFactory::GetForProfile(GetProfile(0));
  event_service->RecordUserEvent(test_event1);
  EXPECT_TRUE(ExpectUserEvents({test_event1}));
  GetSyncService(0)->GetUserSettings()->SetEncryptionPassphrase("passphrase");
  ASSERT_TRUE(PassphraseAcceptedChecker(GetSyncService(0)).Wait());
  event_service->RecordUserEvent(test_event2);

  // Just checking that we don't see test_event2 isn't very convincing yet,
  // because it may simply not have reached the server yet. So let's send
  // something else through the system that we can wait on before checking.
  // Tab/SESSIONS data was picked fairly arbitrarily, note that we expect 2
  // entries, one for the window/header and one for the tab.
  sessions_helper::OpenTab(0, GURL("http://www.one.com/"));
  EXPECT_TRUE(ServerCountMatchStatusChecker(syncer::SESSIONS, 2).Wait());
  EXPECT_TRUE(ExpectUserEvents({test_event1}));
}

IN_PROC_BROWSER_TEST_F(SingleClientUserEventsSyncTest,
                       ShouldNotUploadInSyncPausedState) {
  const UserEventSpecifics test_event =
      CreateTestEvent(base::Time() + base::TimeDelta::FromMicroseconds(1));

  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(GetSyncService(0)->IsSyncFeatureActive());

  // Enter the sync paused state.
  GetClient(0)->EnterSyncPausedStateForPrimaryAccount();
  ASSERT_TRUE(GetSyncService(0)->GetAuthError().IsPersistentError());

  syncer::UserEventService* event_service =
      browser_sync::UserEventServiceFactory::GetForProfile(GetProfile(0));
  event_service->RecordUserEvent(test_event);

  // Clear the "Sync paused" state again.
  GetClient(0)->ExitSyncPausedStateForPrimaryAccount();
  ASSERT_TRUE(GetSyncService(0)->IsSyncFeatureActive());

  // Just checking that we don't see test_event isn't very convincing yet,
  // because it may simply not have reached the server yet. So let's send
  // something else through the system that we can wait on before checking.
  ASSERT_TRUE(
      bookmarks_helper::AddURL(0, "What are you syncing about?",
                               GURL("https://google.com/synced-bookmark-1")));
  ASSERT_TRUE(ServerCountMatchStatusChecker(syncer::BOOKMARKS, 1).Wait());

  // No event should get synced up.
  EXPECT_TRUE(ExpectUserEvents({}));
}

}  // namespace
