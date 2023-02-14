// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/sync/test/integration/session_hierarchy_match_checker.h"
#include "chrome/browser/sync/test/integration/sessions_helper.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/updated_progress_marker_checker.h"
#include "chrome/common/webui_url_constants.h"
#include "components/sync/driver/glue/sync_transport_data_prefs.h"
#include "components/sync/driver/sync_service_impl.h"
#include "components/sync/engine/polling_constants.h"
#include "components/sync/protocol/client_commands.pb.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gmock/include/gmock/gmock.h"

using sessions_helper::CheckInitialState;
using sessions_helper::OpenTab;
using testing::Eq;
using testing::Ge;
using testing::Le;

namespace {

class SingleClientPollingSyncTest : public SyncTest {
 public:
  SingleClientPollingSyncTest() : SyncTest(SINGLE_CLIENT) {}

  SingleClientPollingSyncTest(const SingleClientPollingSyncTest&) = delete;
  SingleClientPollingSyncTest& operator=(const SingleClientPollingSyncTest&) =
      delete;

  ~SingleClientPollingSyncTest() override = default;

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
    SyncTest::SetUpOnMainThread();
  }
};

// This test verifies that the poll interval in prefs gets initialized if no
// data is available yet.
IN_PROC_BROWSER_TEST_F(SingleClientPollingSyncTest, ShouldInitializePollPrefs) {
  // Setup clients and verify no poll interval is present yet.
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  syncer::SyncTransportDataPrefs transport_data_prefs(
      GetProfile(0)->GetPrefs());
  EXPECT_TRUE(transport_data_prefs.GetPollInterval().is_zero());
  ASSERT_TRUE(transport_data_prefs.GetLastPollTime().is_null());

  // Execute a sync cycle and verify the client set up (and persisted) the
  // default value.
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  EXPECT_THAT(transport_data_prefs.GetPollInterval(),
              Eq(syncer::kDefaultPollInterval));
}

// This test verifies that updates of the poll interval get persisted
// That's important make sure clients with short live times will eventually poll
// (e.g. Android).
IN_PROC_BROWSER_TEST_F(SingleClientPollingSyncTest,
                       PRE_ShouldUsePollIntervalFromPrefs) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  sync_pb::ClientCommand client_command;
  client_command.set_set_sync_poll_interval(67);
  GetFakeServer()->SetClientCommand(client_command);

  // Trigger a sync-cycle.
  ASSERT_TRUE(CheckInitialState(0));
  const GURL url = embedded_test_server()->GetURL("/sync/simple.html");
  ASSERT_TRUE(OpenTab(0, url));
  SessionHierarchyMatchChecker checker(
      fake_server::SessionsHierarchy({{url.spec()}}), GetSyncService(0),
      GetFakeServer());
  ASSERT_TRUE(checker.Wait());

  syncer::SyncTransportDataPrefs transport_data_prefs(
      GetProfile(0)->GetPrefs());
  EXPECT_THAT(transport_data_prefs.GetPollInterval().InSeconds(), Eq(67));
}

IN_PROC_BROWSER_TEST_F(SingleClientPollingSyncTest,
                       ShouldUsePollIntervalFromPrefs) {
  // Execute a sync cycle and verify this cycle used that interval.
  // This test assumes the SyncScheduler reads the actual interval from the
  // context. This is covered in the SyncSchedulerImpl's unittest.
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  EXPECT_THAT(GetClient(0)->GetLastCycleSnapshot().poll_interval().InSeconds(),
              Eq(67));
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

  syncer::SyncTransportDataPrefs remote_prefs(GetProfile(0)->GetPrefs());
  // Set small polling interval to make random delays introduced in
  // SyncSchedulerImpl::ComputeLastPollOnStart() negligible, but big enough to
  // avoid periodic polls during a test run.
  remote_prefs.SetPollInterval(base::Seconds(300));

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // Trigger a sync-cycle.
  ASSERT_TRUE(CheckInitialState(0));
  const GURL url = embedded_test_server()->GetURL("/sync/simple.html");
  ASSERT_TRUE(OpenTab(0, url));
  ASSERT_TRUE(SessionHierarchyMatchChecker(
                  fake_server::SessionsHierarchy({{url.spec()}}),
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
  ASSERT_TRUE(GetClient(0)->AwaitEngineInitialization());

  syncer::SyncTransportDataPrefs remote_prefs(GetProfile(0)->GetPrefs());
  ASSERT_FALSE(remote_prefs.GetLastPollTime().is_null());

  // After restart, the last sync cycle snapshot should be empty.
  // Once a sync request happened (e.g. by a poll), that snapshot is populated.
  // We use the following checker to simply wait for an non-empty snapshot.
  EXPECT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());
}

}  // namespace
