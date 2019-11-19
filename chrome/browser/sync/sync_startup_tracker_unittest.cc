// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sync_startup_tracker.h"

#include "components/sync/driver/test_sync_service.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Mock;

namespace {

class MockObserver : public SyncStartupTracker::Observer {
 public:
  MOCK_METHOD0(SyncStartupCompleted, void());
  MOCK_METHOD0(SyncStartupFailed, void());
};

class SyncStartupTrackerTest : public testing::Test {
 public:
  SyncStartupTrackerTest() {}

  void SetupNonInitializedPSS() {
    sync_service_.SetDisableReasons(syncer::SyncService::DISABLE_REASON_NONE);
    sync_service_.SetTransportState(
        syncer::SyncService::TransportState::INITIALIZING);
  }

  syncer::TestSyncService sync_service_;
  MockObserver observer_;
};

TEST_F(SyncStartupTrackerTest, SyncAlreadyInitialized) {
  sync_service_.SetDisableReasons(syncer::SyncService::DISABLE_REASON_NONE);
  sync_service_.SetTransportState(syncer::SyncService::TransportState::ACTIVE);
  EXPECT_CALL(observer_, SyncStartupCompleted());
  SyncStartupTracker tracker(&sync_service_, &observer_);
}

TEST_F(SyncStartupTrackerTest, SyncNotSignedIn) {
  // Make sure that we get a SyncStartupFailed() callback if sync is not logged
  // in.
  sync_service_.SetDisableReasons(
      syncer::SyncService::DISABLE_REASON_NOT_SIGNED_IN);
  sync_service_.SetTransportState(
      syncer::SyncService::TransportState::DISABLED);
  EXPECT_CALL(observer_, SyncStartupFailed());
  SyncStartupTracker tracker(&sync_service_, &observer_);
}

TEST_F(SyncStartupTrackerTest, SyncAuthError) {
  // Make sure that we get a SyncStartupFailed() callback if sync gets an auth
  // error.
  sync_service_.SetDisableReasons(syncer::SyncService::DISABLE_REASON_NONE);
  sync_service_.SetTransportState(
      syncer::SyncService::TransportState::INITIALIZING);
  sync_service_.SetAuthError(
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
  EXPECT_CALL(observer_, SyncStartupFailed());
  SyncStartupTracker tracker(&sync_service_, &observer_);
}

TEST_F(SyncStartupTrackerTest, SyncDelayedInitialization) {
  // Non-initialized PSS should result in no callbacks to the observer.
  SetupNonInitializedPSS();
  EXPECT_CALL(observer_, SyncStartupCompleted()).Times(0);
  EXPECT_CALL(observer_, SyncStartupFailed()).Times(0);
  SyncStartupTracker tracker(&sync_service_, &observer_);
  Mock::VerifyAndClearExpectations(&observer_);
  // Now, mark the PSS as initialized.
  sync_service_.SetTransportState(syncer::SyncService::TransportState::ACTIVE);
  EXPECT_CALL(observer_, SyncStartupCompleted());
  tracker.OnStateChanged(&sync_service_);
}

TEST_F(SyncStartupTrackerTest, SyncDelayedAuthError) {
  // Non-initialized PSS should result in no callbacks to the observer.
  SetupNonInitializedPSS();
  EXPECT_CALL(observer_, SyncStartupCompleted()).Times(0);
  EXPECT_CALL(observer_, SyncStartupFailed()).Times(0);
  SyncStartupTracker tracker(&sync_service_, &observer_);
  Mock::VerifyAndClearExpectations(&observer_);
  Mock::VerifyAndClearExpectations(&sync_service_);

  // Now, mark the PSS as having an auth error.
  sync_service_.SetDisableReasons(syncer::SyncService::DISABLE_REASON_NONE);
  sync_service_.SetTransportState(
      syncer::SyncService::TransportState::INITIALIZING);
  sync_service_.SetAuthError(
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
  EXPECT_CALL(observer_, SyncStartupFailed());
  tracker.OnStateChanged(&sync_service_);
}

TEST_F(SyncStartupTrackerTest, SyncDelayedUnrecoverableError) {
  // Non-initialized PSS should result in no callbacks to the observer.
  SetupNonInitializedPSS();
  EXPECT_CALL(observer_, SyncStartupCompleted()).Times(0);
  EXPECT_CALL(observer_, SyncStartupFailed()).Times(0);
  SyncStartupTracker tracker(&sync_service_, &observer_);
  Mock::VerifyAndClearExpectations(&observer_);
  Mock::VerifyAndClearExpectations(&sync_service_);

  // Now, mark the PSS as having an unrecoverable error.
  sync_service_.SetDisableReasons(
      syncer::SyncService::DISABLE_REASON_UNRECOVERABLE_ERROR);
  sync_service_.SetTransportState(
      syncer::SyncService::TransportState::DISABLED);
  EXPECT_CALL(observer_, SyncStartupFailed());
  tracker.OnStateChanged(&sync_service_);
}

}  // namespace
