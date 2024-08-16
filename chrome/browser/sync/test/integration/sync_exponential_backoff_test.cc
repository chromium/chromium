// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/sync/test/integration/bookmarks_helper.h"
#include "chrome/browser/sync/test/integration/exponential_backoff_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/updated_progress_marker_checker.h"
#include "components/sync/service/sync_service_impl.h"
#include "components/sync/test/fake_server_http_post_provider.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/network_connection_change_simulator.h"
#include "net/base/network_change_notifier.h"

namespace {

using bookmarks_helper::AddFolder;
using bookmarks_helper::ServerBookmarksEqualityChecker;
using exponential_backoff_helper::ExponentialBackoffChecker;

class SyncExponentialBackoffTest : public SyncTest {
 public:
  SyncExponentialBackoffTest() : SyncTest(SINGLE_CLIENT) {}

  SyncExponentialBackoffTest(const SyncExponentialBackoffTest&) = delete;
  SyncExponentialBackoffTest& operator=(const SyncExponentialBackoffTest&) =
      delete;

  ~SyncExponentialBackoffTest() override = default;

  void SetUp() override {
    // This is needed to avoid spurious notifications initiated by the platform.
    net::NetworkChangeNotifier::SetTestNotificationsOnly(true);
    SyncTest::SetUp();
  }
};

// TODO(crbug.com/40854025): Test fails on Lacros.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE_OfflineToOnline DISABLED_OfflineToOnline
#else
#define MAYBE_OfflineToOnline OfflineToOnline
#endif
IN_PROC_BROWSER_TEST_F(SyncExponentialBackoffTest, MAYBE_OfflineToOnline) {
  const std::string kFolderTitle1 = "folder1";
  const std::string kFolderTitle2 = "folder2";

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // Add an item and ensure that sync is successful.
  ASSERT_TRUE(AddFolder(0, 0, kFolderTitle1));
  std::vector<ServerBookmarksEqualityChecker::ExpectedBookmark>
      expected_bookmarks = {{kFolderTitle1, GURL()}};
  ASSERT_TRUE(ServerBookmarksEqualityChecker(expected_bookmarks,
                                             /*cryptographer=*/nullptr)
                  .Wait());

  fake_server::FakeServerHttpPostProvider::DisableNetwork();

  // Add a new item to trigger another sync cycle.
  ASSERT_TRUE(AddFolder(0, 0, kFolderTitle2));

  // Verify that the client goes into exponential backoff while it is unable to
  // reach the sync server.
  ASSERT_TRUE(ExponentialBackoffChecker(GetSyncService(0)).Wait());

  // Double check that the folder hasn't been committed.
  ASSERT_EQ(
      1u, GetFakeServer()->GetSyncEntitiesByDataType(syncer::BOOKMARKS).size());

  // Trigger network change notification and remember time when it happened.
  // Ensure that scheduler runs canary job immediately.
  fake_server::FakeServerHttpPostProvider::EnableNetwork();
  content::NetworkConnectionChangeSimulator connection_change_simulator;
  connection_change_simulator.SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_ETHERNET);

  base::Time network_notification_time = base::Time::Now();

  // Verify that sync was able to recover.
  expected_bookmarks.push_back({kFolderTitle2, GURL()});
  EXPECT_TRUE(ServerBookmarksEqualityChecker(expected_bookmarks,
                                             /*cryptographer=*/nullptr)
                  .Wait());

  // Verify that recovery time is short. Without canary job recovery time would
  // be more than 5 seconds.
  base::TimeDelta recovery_time =
      GetSyncService(0)->GetLastCycleSnapshotForDebugging().sync_start_time() -
      network_notification_time;
  EXPECT_LE(recovery_time, base::Seconds(2));
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
