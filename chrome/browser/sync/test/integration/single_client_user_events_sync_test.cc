// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/metrics/statistics_recorder.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/sync/test/integration/bookmarks_helper.h"
#include "chrome/browser/sync/test/integration/encryption_helper.h"
#include "chrome/browser/sync/test/integration/fake_server_match_status_checker.h"
#include "chrome/browser/sync/test/integration/sessions_helper.h"
#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"
#include "chrome/browser/sync/test/integration/status_change_checker.h"
#include "chrome/browser/sync/test/integration/sync_integration_test_util.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/user_events_helper.h"
#include "chrome/browser/sync/user_event_service_factory.h"
#include "components/sync/base/features.h"
#include "components/sync/protocol/user_event_specifics.pb.h"
#include "components/sync_user_events/user_event_service.h"
#include "content/public/test/browser_test.h"

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
  SingleClientUserEventsSyncTest() : SyncTest(SINGLE_CLIENT) {}

  ~SingleClientUserEventsSyncTest() override = default;

  bool ExpectUserEvents(std::vector<UserEventSpecifics> expected_specifics) {
    return UserEventEqualityChecker(GetSyncService(0), GetFakeServer(),
                                    expected_specifics)
        .Wait();
  }
};

IN_PROC_BROWSER_TEST_F(SingleClientUserEventsSyncTest, Sanity) {
  ASSERT_TRUE(SetupSync());
  EXPECT_EQ(
      0u,
      GetFakeServer()->GetSyncEntitiesByDataType(syncer::USER_EVENTS).size());
  syncer::UserEventService* event_service =
      browser_sync::UserEventServiceFactory::GetForProfile(GetProfile(0));
  const UserEventSpecifics specifics = CreateTestEvent(base::Time());
  event_service->RecordUserEvent(specifics);
  EXPECT_TRUE(ExpectUserEvents({specifics}));
}

IN_PROC_BROWSER_TEST_F(SingleClientUserEventsSyncTest, RetrySequential) {
  ASSERT_TRUE(SetupSync());
  const UserEventSpecifics specifics1 =
      CreateTestEvent(base::Time() + base::Microseconds(1));
  const UserEventSpecifics specifics2 =
      CreateTestEvent(base::Time() + base::Microseconds(2));
  syncer::UserEventService* event_service =
      browser_sync::UserEventServiceFactory::GetForProfile(GetProfile(0));

  GetFakeServer()->OverrideResponseType(
      base::BindRepeating(&BounceType, CommitResponse::TRANSIENT_ERROR));
  event_service->RecordUserEvent(specifics1);

  // This will block until we hit a TRANSIENT_ERROR, at which point we will
  // regain control and can switch back to SUCCESS. Note that the fake server
  // records commits even on failure.
  ASSERT_TRUE(ExpectUserEvents({specifics1}));

  // Wait for another commit attempt and return SUCCESS.
  base::RunLoop run_loop;
  UserEventSpecifics retry_specifics;
  GetFakeServer()->OverrideResponseType(base::BindLambdaForTesting(
      [&](const syncer::LoopbackServerEntity& entity) {
        if (entity.GetDataType() == syncer::USER_EVENTS) {
          retry_specifics = entity.GetSpecifics().user_event();
          run_loop.Quit();
        }
        return CommitResponse::SUCCESS;
      }));
  run_loop.Run();
  EXPECT_EQ(retry_specifics.event_time_usec(), specifics1.event_time_usec());

  // Only record |specifics2| after |specifics1| was successful to avoid race
  // conditions.
  event_service->RecordUserEvent(specifics2);
  EXPECT_TRUE(ExpectUserEvents({specifics1, specifics2}));
}

IN_PROC_BROWSER_TEST_F(SingleClientUserEventsSyncTest, RetryParallel) {
  ASSERT_TRUE(SetupSync());

  const UserEventSpecifics specifics1 =
      CreateTestEvent(base::Time() + base::Microseconds(1));
  const UserEventSpecifics specifics2 =
      CreateTestEvent(base::Time() + base::Microseconds(2));

  syncer::UserEventService* event_service =
      browser_sync::UserEventServiceFactory::GetForProfile(GetProfile(0));

  // Set up the server so that the first commit attempt of `specifics1` results
  // in a transient error.
  base::RunLoop run_loop;
  bool first_attempt = true;
  GetFakeServer()->OverrideResponseType(base::BindLambdaForTesting(
      [&](const syncer::LoopbackServerEntity& entity) {
        if (entity.GetDataType() != syncer::USER_EVENTS ||
            entity.GetSpecifics().user_event().event_time_usec() !=
                specifics1.event_time_usec()) {
          return CommitResponse::SUCCESS;
        }

        if (first_attempt) {
          first_attempt = false;
          return CommitResponse::TRANSIENT_ERROR;
        }

        // Entity has been retried.
        run_loop.Quit();
        return CommitResponse::SUCCESS;
      }));

  event_service->RecordUserEvent(specifics2);
  event_service->RecordUserEvent(specifics1);

  // We can't use only ExpectUserEvents() here because the entity that got the
  // transient error is still considered committed by the fake server.
  run_loop.Run();

  // Verify that both specifics were committed to the server.
  EXPECT_TRUE(ExpectUserEvents({specifics1, specifics2}));
}

IN_PROC_BROWSER_TEST_F(SingleClientUserEventsSyncTest, NoHistory) {
  const UserEventSpecifics test_event1 =
      CreateTestEvent(base::Time() + base::Microseconds(1));
  const UserEventSpecifics test_event2 =
      CreateTestEvent(base::Time() + base::Microseconds(2));
  const UserEventSpecifics test_event3 =
      CreateTestEvent(base::Time() + base::Microseconds(3));

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
      CreateTestEvent(base::Time() + base::Microseconds(1));
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(
      GetClient(0)->DisableSyncForType(syncer::UserSelectableType::kTabs));
  syncer::UserEventService* event_service =
      browser_sync::UserEventServiceFactory::GetForProfile(GetProfile(0));

  event_service->RecordUserEvent(specifics);

  // UserSelectableType::kTabs shouldn't affect UserEvents in any way.
  EXPECT_TRUE(ExpectUserEvents({specifics}));
}

IN_PROC_BROWSER_TEST_F(SingleClientUserEventsSyncTest, Encryption) {
  const UserEventSpecifics test_event1 =
      CreateTestEvent(base::Time() + base::Microseconds(1));
  const UserEventSpecifics test_event2 =
      CreateTestEvent(base::Time() + base::Microseconds(2));

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
      CreateTestEvent(base::Time() + base::Microseconds(1));

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

// This is an analogy to SingleClientBookmarksSyncTest.DepleteQuota, tested on
// a datatype that has no quota restrictions.
IN_PROC_BROWSER_TEST_F(SingleClientUserEventsSyncTest, NoQuotaApplied) {
  ASSERT_TRUE(SetupSync());
  // Add enough user events that would deplete quota in the initial cycle.
  syncer::UserEventService* event_service =
      browser_sync::UserEventServiceFactory::GetForProfile(GetProfile(0));

  std::vector<UserEventSpecifics> expected_specifics;
  base::Time zero;
  // Default number of entities per message on the client are 25, thus the quota
  // would be fully depleted in 25*101 messages.
  for (int i = 0; i < 2525; i++) {
    const UserEventSpecifics specifics =
        CreateTestEvent(zero + base::Milliseconds(i));
    event_service->RecordUserEvent(specifics);
    expected_specifics.push_back(specifics);
  }
  EXPECT_TRUE(ExpectUserEvents(expected_specifics));

  base::HistogramTester histogram_tester;

  // Adding another entity again triggers sync immediately (as there's no
  // quota).
  const UserEventSpecifics specifics = CreateTestEvent(zero + base::Seconds(3));
  event_service->RecordUserEvent(specifics);
  expected_specifics.push_back(specifics);
  EXPECT_TRUE(ExpectUserEvents(expected_specifics));

  // Make sure the histogram gets propagated from the sync engine sequence.
  base::StatisticsRecorder::ImportProvidedHistogramsSync();
  // There is no record in the depleted quota histogram.
  histogram_tester.ExpectTotalCount("Sync.DataTypeCommitWithDepletedQuota", 0);
}

}  // namespace
