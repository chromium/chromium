// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/macros.h"
#include "chrome/browser/sync/test/integration/bookmarks_helper.h"
#include "chrome/browser/sync/test/integration/retry_verifier.h"
#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/updated_progress_marker_checker.h"
#include "components/sync/driver/profile_sync_service.h"
#include "components/sync/test/fake_server/fake_server_http_post_provider.h"
#include "content/public/test/network_connection_change_simulator.h"
#include "net/base/network_change_notifier.h"

namespace {

using bookmarks_helper::AddFolder;
using bookmarks_helper::ModelMatchesVerifier;
using syncer::SyncCycleSnapshot;

class SyncExponentialBackoffTest : public SyncTest {
 public:
  SyncExponentialBackoffTest() : SyncTest(SINGLE_CLIENT) {}
  ~SyncExponentialBackoffTest() override {}

  void SetUp() override {
    // This is needed to avoid spurious notifications initiated by the platform.
    net::NetworkChangeNotifier::SetTestNotificationsOnly(true);
    SyncTest::SetUp();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SyncExponentialBackoffTest);
};

// Helper class that checks if a sync client has successfully gone through
// exponential backoff after it encounters an error.
class ExponentialBackoffChecker : public SingleClientStatusChangeChecker {
 public:
  explicit ExponentialBackoffChecker(syncer::ProfileSyncService* pss)
      : SingleClientStatusChangeChecker(pss) {
    const SyncCycleSnapshot& snap =
        service()->GetLastCycleSnapshotForDebugging();
    retry_verifier_.Initialize(snap);
  }

  // Checks if backoff is complete. Called repeatedly each time PSS notifies
  // observers of a state change.
  bool IsExitConditionSatisfied(std::ostream* os) override {
    *os << "Verifying backoff intervals (" << retry_verifier_.retry_count()
        << "/" << RetryVerifier::kMaxRetry << ")";

    const SyncCycleSnapshot& snap =
        service()->GetLastCycleSnapshotForDebugging();
    retry_verifier_.VerifyRetryInterval(snap);
    return (retry_verifier_.done() && retry_verifier_.Succeeded());
  }

 private:
  // Keeps track of the number of attempts at exponential backoff and its
  // related bookkeeping information for verification.
  RetryVerifier retry_verifier_;

  DISALLOW_COPY_AND_ASSIGN(ExponentialBackoffChecker);
};

IN_PROC_BROWSER_TEST_F(SyncExponentialBackoffTest, OfflineToOnline) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // Add an item and ensure that sync is successful.
  ASSERT_TRUE(AddFolder(0, 0, "folder1"));
  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());

  fake_server::FakeServerHttpPostProvider::DisableNetwork();

  // Add a new item to trigger another sync cycle.
  ASSERT_TRUE(AddFolder(0, 0, "folder2"));

  // Verify that the client goes into exponential backoff while it is unable to
  // reach the sync server.
  ASSERT_TRUE(ExponentialBackoffChecker(GetSyncService(0)).Wait());

  // Trigger network change notification and remember time when it happened.
  // Ensure that scheduler runs canary job immediately.
  fake_server::FakeServerHttpPostProvider::EnableNetwork();
  content::NetworkConnectionChangeSimulator connection_change_simulator;
  connection_change_simulator.SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_ETHERNET);

  base::Time network_notification_time = base::Time::Now();

  // Verify that sync was able to recover.
  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());
  ASSERT_TRUE(ModelMatchesVerifier(0));

  // Verify that recovery time is short. Without canary job recovery time would
  // be more than 5 seconds.
  base::TimeDelta recovery_time =
      GetSyncService(0)->GetLastCycleSnapshotForDebugging().sync_start_time() -
      network_notification_time;
  ASSERT_LE(recovery_time, base::TimeDelta::FromSeconds(2));
}

IN_PROC_BROWSER_TEST_F(SyncExponentialBackoffTest, ServerRedirect) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // Add an item and ensure that sync is successful.
  ASSERT_TRUE(AddFolder(0, 0, "folder1"));
  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());

  GetFakeServer()->SetHttpError(net::HTTP_USE_PROXY);

  // Add a new item to trigger another sync cycle.
  ASSERT_TRUE(AddFolder(0, 0, "folder2"));

  // Verify that the client goes into exponential backoff while it is unable to
  // reach the sync server.
  ASSERT_TRUE(ExponentialBackoffChecker(GetSyncService(0)).Wait());
}

IN_PROC_BROWSER_TEST_F(SyncExponentialBackoffTest, InternalServerError) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // Add an item and ensure that sync is successful.
  ASSERT_TRUE(AddFolder(0, 0, "folder1"));
  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());

  GetFakeServer()->SetHttpError(net::HTTP_INTERNAL_SERVER_ERROR);

  // Add a new item to trigger another sync cycle.
  ASSERT_TRUE(AddFolder(0, 0, "folder2"));

  // Verify that the client goes into exponential backoff while it is unable to
  // reach the sync server.
  ASSERT_TRUE(ExponentialBackoffChecker(GetSyncService(0)).Wait());
}

IN_PROC_BROWSER_TEST_F(SyncExponentialBackoffTest, TransientErrorTest) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // Add an item and ensure that sync is successful.
  ASSERT_TRUE(AddFolder(0, 0, "folder1"));
  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());

  GetFakeServer()->TriggerError(sync_pb::SyncEnums::TRANSIENT_ERROR);

  // Add a new item to trigger another sync cycle.
  ASSERT_TRUE(AddFolder(0, 0, "folder2"));

  // Verify that the client goes into exponential backoff while it is unable to
  // reach the sync server.
  ASSERT_TRUE(ExponentialBackoffChecker(GetSyncService(0)).Wait());
}

}  // namespace
