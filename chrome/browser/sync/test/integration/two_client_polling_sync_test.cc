// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/session_hierarchy_match_checker.h"
#include "chrome/browser/sync/test/integration/sessions_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/common/webui_url_constants.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/sync/base/sync_prefs.h"
#include "components/sync/driver/profile_sync_service.h"
#include "components/sync/engine/polling_constants.h"
#include "components/sync/protocol/client_commands.pb.h"
#include "components/sync/test/fake_server/sessions_hierarchy.h"
#include "testing/gmock/include/gmock/gmock.h"

using sessions_helper::CheckInitialState;
using sessions_helper::OpenTab;
using syncer::SyncPrefs;
using testing::Gt;

namespace {

const char kURL1[] = "data:text/html,<html><title>Test</title></html>";

class TwoClientPollingSyncTest : public SyncTest {
 public:
  TwoClientPollingSyncTest() : SyncTest(TWO_CLIENT) {}
  ~TwoClientPollingSyncTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TwoClientPollingSyncTest);
};

class SessionCountMatchChecker : public SingleClientStatusChangeChecker {
 public:
  SessionCountMatchChecker(int expected_count,
                           syncer::ProfileSyncService* service,
                           fake_server::FakeServer* fake_server)
      : SingleClientStatusChangeChecker(service),
        expected_count_(expected_count),
        verifier_(fake_server) {}

  // StatusChangeChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override {
    *os << "Waiting for a matching number of sessions to be refleted on the "
           "fake server.";
    return verifier_.VerifyEntityCountByType(expected_count_, syncer::SESSIONS);
  }

 private:
  const int expected_count_;
  fake_server::FakeServerVerifier verifier_;
};

// This test writes from one client (0) and makes sure the data arrives
// on a remote client (1) even if notifications don't work.
// Because the initial run of sync is doing a number of extra sync cycles,
// this test is structured in 2 phases. In the first phase, we simply bring
// up two clients and have them sync some data.
// In the second phase, we take down client 1 and while it's down upload more
// data from client 0. That second phase will rely on polling on client 1 to
// receive the update.
// Flaky: crbug.com/988161
IN_PROC_BROWSER_TEST_F(TwoClientPollingSyncTest, DISABLED_ShouldPollOnStartup) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  // Choose larger interval to verify the poll-on-start logic.
  SyncPrefs remote_prefs(GetProfile(1)->GetPrefs());
  remote_prefs.SetPollInterval(base::TimeDelta::FromMinutes(2));

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // Disable syncing of AUTOFILL_WALLET_DATA: That type is special-cased to
  // clear its data even with KEEP_DATA, which means we'd always send a regular
  // GetUpdates request on starting Sync again, and so we'd have no need for a
  // poll.
  GetClient(0)->DisableSyncForType(syncer::UserSelectableType::kAutofill);
  GetClient(1)->DisableSyncForType(syncer::UserSelectableType::kAutofill);
  // TODO(crbug.com/890737): Once AUTOFILL_WALLET_DATA gets properly disabled
  // based on the pref, we can just disable that instead of all of AUTOFILL:
  // autofill::prefs::SetPaymentsIntegrationEnabled(GetProfile(0)->GetPrefs(),
  //                                                false);
  // autofill::prefs::SetPaymentsIntegrationEnabled(GetProfile(1)->GetPrefs(),
  //                                                false);

  // Phase 1.
  ASSERT_TRUE(CheckInitialState(0));
  ASSERT_TRUE(CheckInitialState(1));
  ASSERT_TRUE(OpenTab(0, GURL(chrome::kChromeUIHistoryURL)));
  GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1));

  ASSERT_FALSE(GetSyncService(0)->GetActiveDataTypes().Has(
      syncer::AUTOFILL_WALLET_DATA));
  ASSERT_FALSE(GetSyncService(1)->GetActiveDataTypes().Has(
      syncer::AUTOFILL_WALLET_DATA));

  // Phase 2.
  // Disconnect client 1 from sync and write another change from client 0.
  // Disconnect the remote client from the invalidation service.
  DisableNotificationsForClient(1);
  // Make sure no extra sync cycles get triggered by test infrastructure.
  StopConfigurationRefresher();
  // Note: It's important to *not* clear data here - if we clear data, then
  // we'll do a regular GetUpdates at the next startup, so there'd be no need
  // for a poll.
  GetClient(1)->StopSyncServiceWithoutClearingData();

  ASSERT_TRUE(OpenTab(0, GURL(kURL1)));
  SessionCountMatchChecker server_checker(4, GetSyncService(0),
                                          GetFakeServer());
  EXPECT_TRUE(server_checker.Wait());

  // Now start up the remote client (make sure it should start a poll after
  // start-up) and verify it receives the latest changes and the poll cycle
  // updated the last-poll-time.
  // All data is already there, so we can get it in the first poll.
  base::Time remote_start = base::Time::Now();
  base::Time new_last_poll_time = base::Time::Now() -
                                  base::TimeDelta::FromMinutes(2) -
                                  base::TimeDelta::FromMilliseconds(100);
  remote_prefs.SetLastPollTime(new_last_poll_time);
  ASSERT_TRUE(GetClient(1)->StartSyncService()) << "SetupSync() failed.";
  GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1));
  EXPECT_THAT(remote_prefs.GetLastPollTime(), Gt(remote_start));
}

}  // namespace
