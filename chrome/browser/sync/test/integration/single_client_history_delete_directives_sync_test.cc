// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/test/bind_test_util.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/web_history_service_factory.h"
#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/updated_progress_marker_checker.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/sync/engine_impl/loopback_server/persistent_unique_client_entity.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/sync/test/fake_server/fake_server.h"
#include "content/public/test/browser_test.h"

namespace {

using sync_pb::HistoryDeleteDirectiveSpecifics;

int64_t TimeToUnixUsec(base::Time time) {
  DCHECK(!time.is_null());
  return (time - base::Time::UnixEpoch()).InMicroseconds();
}

// Allows to wait until the number of server-side entities is equal to a
// expected number.
class HistoryDeleteDirectivesEqualityChecker
    : public SingleClientStatusChangeChecker {
 public:
  HistoryDeleteDirectivesEqualityChecker(syncer::ProfileSyncService* service,
                                         fake_server::FakeServer* fake_server,
                                         size_t num_expected_directives)
      : SingleClientStatusChangeChecker(service),
        fake_server_(fake_server),
        num_expected_directives_(num_expected_directives) {}

  bool IsExitConditionSatisfied(std::ostream* os) override {
    *os << "Waiting server side HISTORY_DELETE_DIRECTIVES to match expected.";
    const std::vector<sync_pb::SyncEntity> entities =
        fake_server_->GetSyncEntitiesByModelType(
            syncer::HISTORY_DELETE_DIRECTIVES);

    if (entities.size() == num_expected_directives_) {
      return true;
    }

    // |entities.size()| is only going to grow, if |entities.size()| ever
    // becomes bigger then all hope is lost of passing, stop now.
    EXPECT_LT(entities.size(), num_expected_directives_)
        << "Entity set will never become equal";
    return false;
  }

 private:
  fake_server::FakeServer* const fake_server_;
  const size_t num_expected_directives_;

  DISALLOW_COPY_AND_ASSIGN(HistoryDeleteDirectivesEqualityChecker);
};

class SingleClientHistoryDeleteDirectivesSyncTest : public SyncTest {
 public:
  SingleClientHistoryDeleteDirectivesSyncTest() : SyncTest(SINGLE_CLIENT) {}

  ~SingleClientHistoryDeleteDirectivesSyncTest() override {}

  bool WaitForHistoryDeleteDirectives(size_t num_expected_directives) {
    return HistoryDeleteDirectivesEqualityChecker(
               GetSyncService(0), GetFakeServer(), num_expected_directives)
        .Wait();
  }

  // Uses HistoryService to look up whether any history entry exists that
  // exactly matches timestamp |time|.
  bool LookupLocalHistoryEntry(base::Time time) {
    history::HistoryService* history_service =
        HistoryServiceFactory::GetForProfileWithoutCreating(GetProfile(0));

    bool exists = false;
    base::RunLoop loop;
    base::CancelableTaskTracker task_tracker;
    history_service->GetHistoryCount(
        /*begin_time=*/time,
        /*end_time=*/time + base::TimeDelta::FromMicroseconds(1),
        base::BindLambdaForTesting([&](history::HistoryCountResult result) {
          ASSERT_TRUE(result.success);
          exists = (result.count != 0);
          loop.Quit();
        }),
        &task_tracker);
    loop.Run();
    return exists;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SingleClientHistoryDeleteDirectivesSyncTest);
};

IN_PROC_BROWSER_TEST_F(SingleClientHistoryDeleteDirectivesSyncTest,
                       ShouldCommitTimeRangeDeleteDirective) {
  const GURL kPageUrl = GURL("http://foo.com");
  const base::Time kHistoryEntryTime = base::Time::Now();
  base::CancelableTaskTracker task_tracker;

  ASSERT_TRUE(SetupSync());

  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfileWithoutCreating(GetProfile(0));
  history_service->AddPage(kPageUrl, kHistoryEntryTime,
                           history::SOURCE_BROWSED);

  history_service->DeleteLocalAndRemoteHistoryBetween(
      WebHistoryServiceFactory::GetForProfile(GetProfile(0)),
      /*begin_time=*/base::Time(), /*end_time=*/base::Time(), base::DoNothing(),
      &task_tracker);

  EXPECT_TRUE(WaitForHistoryDeleteDirectives(1));
}

IN_PROC_BROWSER_TEST_F(SingleClientHistoryDeleteDirectivesSyncTest,
                       ShouldCommitUrlDeleteDirective) {
  const GURL kPageUrl = GURL("http://foo.com");
  const base::Time kHistoryEntryTime = base::Time::Now();
  ASSERT_TRUE(SetupSync());

  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfileWithoutCreating(GetProfile(0));
  history_service->AddPage(kPageUrl, kHistoryEntryTime,
                           history::SOURCE_BROWSED);

  history_service->DeleteLocalAndRemoteUrl(
      WebHistoryServiceFactory::GetForProfile(GetProfile(0)), kPageUrl);

  EXPECT_TRUE(WaitForHistoryDeleteDirectives(1));
}

IN_PROC_BROWSER_TEST_F(SingleClientHistoryDeleteDirectivesSyncTest,
                       ShouldProcessDeleteDirectiveDuringStartup) {
  const GURL kPageUrl = GURL("http://foo.com");
  const base::Time kHistoryEntryTime = base::Time::Now();

  ASSERT_TRUE(SetupClients());

  // Initially (before sync starts) there is a local history entry.
  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfileWithoutCreating(GetProfile(0));
  history_service->AddPage(kPageUrl, kHistoryEntryTime,
                           history::SOURCE_BROWSED);

  // Initially (before sync starts) there is a remote delete directive.
  sync_pb::EntitySpecifics specifics;
  sync_pb::TimeRangeDirective* time_range_directive =
      specifics.mutable_history_delete_directive()
          ->mutable_time_range_directive();
  time_range_directive->set_start_time_usec(TimeToUnixUsec(kHistoryEntryTime));
  time_range_directive->set_end_time_usec(TimeToUnixUsec(kHistoryEntryTime) +
                                          1);

  fake_server_->InjectEntity(
      syncer::PersistentUniqueClientEntity::CreateFromSpecificsForTesting(
          "non_unique_name", "client_tag", specifics, /*creation_time=*/0,
          /*last_modified_time=*/0));
  EXPECT_TRUE(WaitForHistoryDeleteDirectives(1));

  // Verify history exists prior to starting sync.
  ASSERT_TRUE(LookupLocalHistoryEntry(kHistoryEntryTime));

  ASSERT_TRUE(SetupSync());

  // Verify history entry was deleted.
  EXPECT_FALSE(LookupLocalHistoryEntry(kHistoryEntryTime));

  // No deletion should be sent to the server. There's no way to verify this
  // reliably in an integration test (i.e. it could eventually be deleted), but
  // this should be a good approximation considering there was a round-trip to
  // the history DB.
  EXPECT_TRUE(WaitForHistoryDeleteDirectives(1));
}

}  // namespace
