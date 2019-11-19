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
#include "chrome/browser/sync/test/integration/updated_progress_marker_checker.h"
#include "chrome/common/webui_url_constants.h"
#include "components/sync/base/sync_prefs.h"
#include "components/sync/driver/profile_sync_service.h"
#include "components/sync/engine/polling_constants.h"
#include "components/sync/protocol/client_commands.pb.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::Eq;
using testing::Ge;
using testing::Le;
using sessions_helper::CheckInitialState;
using sessions_helper::OpenTab;
using syncer::SyncPrefs;

namespace {

class SingleClientPollingSyncTest : public SyncTest {
 public:
  SingleClientPollingSyncTest() : SyncTest(SINGLE_CLIENT) {}
  ~SingleClientPollingSyncTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SingleClientPollingSyncTest);
};

// This test verifies that the poll interval in prefs gets initialized if no
// data is available yet.
IN_PROC_BROWSER_TEST_F(SingleClientPollingSyncTest, ShouldInitializePollPrefs) {
  // Setup clients and verify no poll interval is present yet.
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  SyncPrefs sync_prefs(GetProfile(0)->GetPrefs());
  EXPECT_TRUE(sync_prefs.GetPollInterval().is_zero());
  ASSERT_TRUE(sync_prefs.GetLastPollTime().is_null());

  // Execute a sync cycle and verify the client set up (and persisted) the
  // default value.
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  EXPECT_THAT(sync_prefs.GetPollInterval().InSeconds(),
              Eq(syncer::kDefaultPollIntervalSeconds));
}

// This test verifies that updates of the poll interval get persisted
// That's important make sure clients with short live times will eventually poll
// (e.g. Android).
IN_PROC_BROWSER_TEST_F(SingleClientPollingSyncTest, ShouldUpdatePollPrefs) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  sync_pb::ClientCommand client_command;
  client_command.set_set_sync_poll_interval(67);
  GetFakeServer()->SetClientCommand(client_command);

  // Trigger a sync-cycle.
  ASSERT_TRUE(CheckInitialState(0));
  ASSERT_TRUE(OpenTab(0, GURL(chrome::kChromeUIHistoryURL)));
  SessionHierarchyMatchChecker checker(
      fake_server::SessionsHierarchy(
          {{GURL(chrome::kChromeUIHistoryURL).spec()}}),
      GetSyncService(0), GetFakeServer());
  ASSERT_TRUE(checker.Wait());

  SyncPrefs sync_prefs(GetProfile(0)->GetPrefs());
  EXPECT_THAT(sync_prefs.GetPollInterval().InSeconds(), Eq(67));
}

IN_PROC_BROWSER_TEST_F(SingleClientPollingSyncTest,
                       ShouldUsePollIntervalFromPrefs) {
  // Setup clients and provide new poll interval via prefs.
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  SyncPrefs sync_prefs(GetProfile(0)->GetPrefs());
  sync_prefs.SetPollInterval(base::TimeDelta::FromSeconds(123));

  // Execute a sync cycle and verify this cycle used that interval.
  // This test assumes the SyncScheduler reads the actual interval from the
  // context. This is covered in the SyncSchedulerImpl's unittest.
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  EXPECT_THAT(GetClient(0)->GetLastCycleSnapshot().poll_interval().InSeconds(),
              Eq(123));
}

// This test simulates the poll interval expiring between restarts.
// It first starts up a client, executes a sync cycle and stops it. After a
// simulated pause, the client gets started up again and we expect a sync cycle
// to happen (which would be caused by polling).
// Note, that there's a more realistic (and more complex) test for this in
// two_client_polling_sync_test.cc too.
IN_PROC_BROWSER_TEST_F(SingleClientPollingSyncTest,
                       PRE_ShouldPollWhenIntervalExpiredAcrossRestarts) {
  base::Time start = base::Time::Now();

  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  SyncPrefs remote_prefs(GetProfile(0)->GetPrefs());
  // Set small polling interval to make random delays introduced in
  // SyncSchedulerImpl::ComputeLastPollOnStart() negligible, but big enough to
  // avoid periodic polls during a test run.
  remote_prefs.SetPollInterval(base::TimeDelta::FromSeconds(300));

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // Trigger a sync-cycle.
  ASSERT_TRUE(CheckInitialState(0));
  ASSERT_TRUE(OpenTab(0, GURL(chrome::kChromeUIHistoryURL)));
  ASSERT_TRUE(SessionHierarchyMatchChecker(
                  fake_server::SessionsHierarchy(
                      {{GURL(chrome::kChromeUIHistoryURL).spec()}}),
                  GetSyncService(0), GetFakeServer())
                  .Wait());

  // Verify that the last poll time got initialized to a reasonable value.
  EXPECT_THAT(remote_prefs.GetLastPollTime(), Ge(start));
  EXPECT_THAT(remote_prefs.GetLastPollTime(), Le(base::Time::Now()));

  // Simulate elapsed time so that the poll interval expired upon restart.
  remote_prefs.SetLastPollTime(base::Time::Now() -
                               remote_prefs.GetPollInterval());
}

IN_PROC_BROWSER_TEST_F(SingleClientPollingSyncTest,
                       ShouldPollWhenIntervalExpiredAcrossRestarts) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
#if defined(CHROMEOS)
  // signin::SetRefreshTokenForPrimaryAccount() is needed on ChromeOS in order
  // to get a non-empty refresh token on startup.
  GetClient(0)->SignInPrimaryAccount();
#endif  // defined(CHROMEOS)
  ASSERT_TRUE(GetClient(0)->AwaitEngineInitialization());

  SyncPrefs remote_prefs(GetProfile(0)->GetPrefs());
  ASSERT_FALSE(remote_prefs.GetLastPollTime().is_null());

  // After restart, the last sync cycle snapshot should be empty.
  // Once a sync request happened (e.g. by a poll), that snapshot is populated.
  // We use the following checker to simply wait for an non-empty snapshot.
  EXPECT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());
}

}  // namespace
