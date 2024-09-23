// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sync_startup_tracker.h"
#include <memory>

#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/sync/test/test_sync_service.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Mock;

namespace {

class SyncStartupTrackerTest : public testing::Test {
 public:
  void SetupNonInitializedSyncService() {
    sync_service_.SetMaxTransportState(
        syncer::SyncService::TransportState::INITIALIZING);
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  syncer::TestSyncService sync_service_;
  base::MockCallback<SyncStartupTracker::SyncStartupStateChangedCallback>
      callback_;
};

TEST_F(SyncStartupTrackerTest, SyncAlreadyInitialized) {
  sync_service_.SetMaxTransportState(
      syncer::SyncService::TransportState::ACTIVE);
  EXPECT_CALL(callback_,
              Run(SyncStartupTracker::ServiceStartupState::kComplete));
  SyncStartupTracker tracker(&sync_service_, callback_.Get());
}

TEST_F(SyncStartupTrackerTest, SyncNotSignedIn) {
  // Make sure that we get a SyncStartupFailed() callback if sync is not logged
  // in.
  sync_service_.SetSignedOut();
  EXPECT_CALL(callback_, Run(SyncStartupTracker::ServiceStartupState::kError));
  SyncStartupTracker tracker(&sync_service_, callback_.Get());
}

TEST_F(SyncStartupTrackerTest, SyncAuthError) {
  // Make sure that we get a SyncStartupFailed() callback if sync gets an auth
  // error.
  sync_service_.SetPersistentAuthError();
  EXPECT_CALL(callback_, Run(SyncStartupTracker::ServiceStartupState::kError));
  SyncStartupTracker tracker(&sync_service_, callback_.Get());
}

TEST_F(SyncStartupTrackerTest, SyncDelayedInitialization) {
  // Non-initialized Sync Service should result in no callbacks to the observer.
  SetupNonInitializedSyncService();
  EXPECT_CALL(callback_, Run(_)).Times(0);
  SyncStartupTracker tracker(&sync_service_, callback_.Get());
  Mock::VerifyAndClearExpectations(&callback_);
  // Now, mark the Sync Service as initialized.
  sync_service_.SetMaxTransportState(
      syncer::SyncService::TransportState::ACTIVE);
  EXPECT_CALL(callback_,
              Run(SyncStartupTracker::ServiceStartupState::kComplete));
  tracker.OnStateChanged(&sync_service_);
}

TEST_F(SyncStartupTrackerTest, SyncDelayedAuthError) {
  // Non-initialized Sync Service should result in no callbacks to the observer.
  SetupNonInitializedSyncService();
  EXPECT_CALL(callback_, Run(_)).Times(0);
  SyncStartupTracker tracker(&sync_service_, callback_.Get());
  Mock::VerifyAndClearExpectations(&callback_);
  Mock::VerifyAndClearExpectations(&sync_service_);

  // Now, mark the Sync Service as having an auth error.
  sync_service_.SetPersistentAuthError();
  EXPECT_CALL(callback_, Run(SyncStartupTracker::ServiceStartupState::kError));
  tracker.OnStateChanged(&sync_service_);
}

TEST_F(SyncStartupTrackerTest, SyncDelayedUnrecoverableError) {
  // Non-initialized Sync Service should result in no callbacks to the observer.
  SetupNonInitializedSyncService();
  EXPECT_CALL(callback_, Run(_)).Times(0);
  SyncStartupTracker tracker(&sync_service_, callback_.Get());
  Mock::VerifyAndClearExpectations(&callback_);
  Mock::VerifyAndClearExpectations(&sync_service_);

  // Now, mark the Sync Service as having an unrecoverable error.
  sync_service_.SetHasUnrecoverableError(true);
  EXPECT_CALL(callback_, Run(SyncStartupTracker::ServiceStartupState::kError));
  tracker.OnStateChanged(&sync_service_);
}

TEST_F(SyncStartupTrackerTest, TrackingCancelled) {
  // Non-initialized Sync Service should result in no callbacks to the observer.
  SetupNonInitializedSyncService();
  EXPECT_CALL(callback_, Run(_)).Times(0);
  std::unique_ptr<SyncStartupTracker> tracker =
      std::make_unique<SyncStartupTracker>(&sync_service_, callback_.Get());
  Mock::VerifyAndClearExpectations(&callback_);

  // Now, cancel the tracker.
  EXPECT_CALL(callback_, Run(_)).Times(0);
  tracker.reset();
}

TEST_F(SyncStartupTrackerTest, SyncTimedOut) {
  // Non-initialized Sync Service should result in no callbacks to the observer.
  SetupNonInitializedSyncService();
  EXPECT_CALL(callback_, Run(_)).Times(0);
  std::unique_ptr<SyncStartupTracker> tracker =
      std::make_unique<SyncStartupTracker>(&sync_service_, callback_.Get());
  Mock::VerifyAndClearExpectations(&callback_);

  // Now, advance the timer.
  EXPECT_CALL(callback_,
              Run(SyncStartupTracker::ServiceStartupState::kTimeout));
  task_environment_.FastForwardBy(base::Seconds(30));
}

}  // namespace
