// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/sync/test/integration/bookmarks_helper.h"
#include "chrome/browser/sync/test/integration/exponential_backoff_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/updated_progress_marker_checker.h"
#include "components/sync/base/features.h"
#include "components/sync/engine/polling_constants.h"
#include "components/sync/service/sync_service_impl.h"
#include "content/public/test/browser_test.h"
#include "net/base/network_change_notifier.h"

namespace {

using bookmarks_helper::AddFolder;
using bookmarks_helper::ServerBookmarksEqualityChecker;
using exponential_backoff_helper::ExponentialBackoffChecker;

class SyncExponentialBackoffTest
    : public SyncTest,
      public testing::WithParamInterface<SyncTest::SetupSyncMode> {
 public:
  SyncExponentialBackoffTest() : SyncTest(SINGLE_CLIENT) {
    if (GetSetupSyncMode() == SetupSyncMode::kSyncTransportOnly) {
      scoped_feature_list_.InitAndEnableFeature(
          syncer::kReplaceSyncPromosWithSignInPromos);
    }
  }

  SyncExponentialBackoffTest(const SyncExponentialBackoffTest&) = delete;
  SyncExponentialBackoffTest& operator=(const SyncExponentialBackoffTest&) =
      delete;

  ~SyncExponentialBackoffTest() override = default;

  void SetUp() override {
    // This is needed to avoid spurious notifications initiated by the platform.
    net::NetworkChangeNotifier::SetTestNotificationsOnly(true);
    SyncTest::SetUp();
  }

  SyncTest::SetupSyncMode GetSetupSyncMode() const override {
    return GetParam();
  }

  bookmarks_helper::StoreType GetBookmarksStoreType() const {
    return GetSetupSyncMode() == SyncTest::SetupSyncMode::kSyncTransportOnly
               ? bookmarks_helper::StoreType::kAccountStore
               : bookmarks_helper::StoreType::kLocalOrSyncableStore;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(,
                         SyncExponentialBackoffTest,
                         GetSyncTestModes(),
                         testing::PrintToStringParamName());

IN_PROC_BROWSER_TEST_P(SyncExponentialBackoffTest, OfflineToOnline) {
  const std::u16string kFolderTitle1 = u"folder1";
  const std::u16string kFolderTitle2 = u"folder2";

  ASSERT_TRUE(SetupSync());

  // Add an item and ensure that sync is successful.
  ASSERT_TRUE(AddFolder(0, 0, kFolderTitle1, GetBookmarksStoreType()));
  std::vector<ServerBookmarksEqualityChecker::ExpectedBookmark>
      expected_bookmarks = {{kFolderTitle1, GURL()}};
  ASSERT_TRUE(ServerBookmarksEqualityChecker(expected_bookmarks,
                                             /*cryptographer=*/nullptr)
                  .Wait());

  DisableNetwork();

  // Add a new item to trigger another sync cycle.
  ASSERT_TRUE(AddFolder(0, 0, kFolderTitle2, GetBookmarksStoreType()));

  // Verify that the client goes into exponential backoff while it is unable to
  // reach the sync server.
  ASSERT_TRUE(ExponentialBackoffChecker(
                  GetSyncService(0), syncer::kInitialBackoffImmediateRetryTime)
                  .Wait());

  // Double check that the folder hasn't been committed.
  ASSERT_EQ(
      1u, GetFakeServer()->GetSyncEntitiesByDataType(syncer::BOOKMARKS).size());

  // Trigger network change notification and remember time when it happened.
  // Ensure that scheduler runs canary job immediately.
  EnableNetwork();

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

IN_PROC_BROWSER_TEST_P(SyncExponentialBackoffTest, ServerRedirect) {
  ASSERT_TRUE(SetupSync());

  // Add an item and ensure that sync is successful.
  ASSERT_TRUE(AddFolder(0, 0, u"folder1", GetBookmarksStoreType()));
  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());

  GetFakeServer()->SetHttpError(net::HTTP_USE_PROXY);

  // Add a new item to trigger another sync cycle.
  ASSERT_TRUE(AddFolder(0, 0, u"folder2", GetBookmarksStoreType()));

  // Verify that the client goes into exponential backoff while it is unable to
  // reach the sync server.
  ASSERT_TRUE(ExponentialBackoffChecker(GetSyncService(0),
                                        syncer::kInitialBackoffShortRetryTime)
                  .Wait());
}

IN_PROC_BROWSER_TEST_P(SyncExponentialBackoffTest, InternalServerError) {
  ASSERT_TRUE(SetupSync());

  // Add an item and ensure that sync is successful.
  ASSERT_TRUE(AddFolder(0, 0, u"folder1", GetBookmarksStoreType()));
  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());

  GetFakeServer()->SetHttpError(net::HTTP_INTERNAL_SERVER_ERROR);

  // Add a new item to trigger another sync cycle.
  ASSERT_TRUE(AddFolder(0, 0, u"folder2", GetBookmarksStoreType()));

  // Verify that the client goes into exponential backoff while it is unable to
  // reach the sync server.
  ASSERT_TRUE(ExponentialBackoffChecker(GetSyncService(0),
                                        syncer::kInitialBackoffShortRetryTime)
                  .Wait());
}

IN_PROC_BROWSER_TEST_P(SyncExponentialBackoffTest, TransientErrorTest) {
  ASSERT_TRUE(SetupSync());

  // Add an item and ensure that sync is successful.
  ASSERT_TRUE(AddFolder(0, 0, u"folder1", GetBookmarksStoreType()));
  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());

  GetFakeServer()->TriggerError(sync_pb::SyncEnums::TRANSIENT_ERROR);

  // Add a new item to trigger another sync cycle.
  ASSERT_TRUE(AddFolder(0, 0, u"folder2", GetBookmarksStoreType()));

  // Verify that the client goes into exponential backoff while it is unable to
  // reach the sync server.
  ASSERT_TRUE(ExponentialBackoffChecker(GetSyncService(0),
                                        syncer::kInitialBackoffShortRetryTime)
                  .Wait());
}

}  // namespace
