// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sync_startup_tracker.h"

#include <memory>

#include "base/bind.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/profile_sync_test_util.h"
#include "chrome/test/base/testing_profile.h"
#include "components/browser_sync/profile_sync_service_mock.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Mock;
using ::testing::Return;
using ::testing::ReturnRef;

namespace {

class MockObserver : public SyncStartupTracker::Observer {
 public:
  MOCK_METHOD0(SyncStartupCompleted, void());
  MOCK_METHOD0(SyncStartupFailed, void());
};

class SyncStartupTrackerTest : public testing::Test {
 public:
  SyncStartupTrackerTest() :
      no_error_(GoogleServiceAuthError::NONE) {
  }
  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    mock_pss_ = static_cast<browser_sync::ProfileSyncServiceMock*>(
        ProfileSyncServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            profile_.get(), base::BindRepeating(&BuildMockProfileSyncService)));

    ON_CALL(*mock_pss_, GetAuthError()).WillByDefault(ReturnRef(no_error_));
    ON_CALL(*mock_pss_, GetRegisteredDataTypes())
        .WillByDefault(Return(syncer::ModelTypeSet()));
    mock_pss_->Initialize();
  }

  void TearDown() override { profile_.reset(); }

  void SetupNonInitializedPSS() {
    ON_CALL(*mock_pss_, GetAuthError()).WillByDefault(ReturnRef(no_error_));
    ON_CALL(*mock_pss_, GetDisableReasons())
        .WillByDefault(Return(syncer::SyncService::DISABLE_REASON_NONE));
    ON_CALL(*mock_pss_, GetTransportState())
        .WillByDefault(
            Return(syncer::SyncService::TransportState::INITIALIZING));
  }

  content::TestBrowserThreadBundle thread_bundle_;
  const GoogleServiceAuthError no_error_;
  std::unique_ptr<TestingProfile> profile_;
  browser_sync::ProfileSyncServiceMock* mock_pss_;
  MockObserver observer_;
};

TEST_F(SyncStartupTrackerTest, SyncAlreadyInitialized) {
  ON_CALL(*mock_pss_, GetDisableReasons())
      .WillByDefault(Return(syncer::SyncService::DISABLE_REASON_NONE));
  ON_CALL(*mock_pss_, GetTransportState())
      .WillByDefault(Return(syncer::SyncService::TransportState::ACTIVE));
  EXPECT_CALL(observer_, SyncStartupCompleted());
  SyncStartupTracker tracker(profile_.get(), &observer_);
}

TEST_F(SyncStartupTrackerTest, SyncNotSignedIn) {
  // Make sure that we get a SyncStartupFailed() callback if sync is not logged
  // in.
  ON_CALL(*mock_pss_, GetDisableReasons())
      .WillByDefault(Return(syncer::SyncService::DISABLE_REASON_NOT_SIGNED_IN));
  ON_CALL(*mock_pss_, GetTransportState())
      .WillByDefault(Return(syncer::SyncService::TransportState::DISABLED));
  EXPECT_CALL(observer_, SyncStartupFailed());
  SyncStartupTracker tracker(profile_.get(), &observer_);
}

TEST_F(SyncStartupTrackerTest, SyncAuthError) {
  // Make sure that we get a SyncStartupFailed() callback if sync gets an auth
  // error.
  ON_CALL(*mock_pss_, GetDisableReasons())
      .WillByDefault(Return(syncer::SyncService::DISABLE_REASON_NONE));
  ON_CALL(*mock_pss_, GetTransportState())
      .WillByDefault(Return(syncer::SyncService::TransportState::INITIALIZING));
  GoogleServiceAuthError error(
      GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
  ON_CALL(*mock_pss_, GetAuthError()).WillByDefault(ReturnRef(error));
  EXPECT_CALL(observer_, SyncStartupFailed());
  SyncStartupTracker tracker(profile_.get(), &observer_);
}

TEST_F(SyncStartupTrackerTest, SyncDelayedInitialization) {
  // Non-initialized PSS should result in no callbacks to the observer.
  SetupNonInitializedPSS();
  EXPECT_CALL(observer_, SyncStartupCompleted()).Times(0);
  EXPECT_CALL(observer_, SyncStartupFailed()).Times(0);
  SyncStartupTracker tracker(profile_.get(), &observer_);
  Mock::VerifyAndClearExpectations(&observer_);
  // Now, mark the PSS as initialized.
  ON_CALL(*mock_pss_, GetTransportState())
      .WillByDefault(Return(syncer::SyncService::TransportState::ACTIVE));
  EXPECT_CALL(observer_, SyncStartupCompleted());
  tracker.OnStateChanged(mock_pss_);
}

TEST_F(SyncStartupTrackerTest, SyncDelayedAuthError) {
  // Non-initialized PSS should result in no callbacks to the observer.
  SetupNonInitializedPSS();
  EXPECT_CALL(observer_, SyncStartupCompleted()).Times(0);
  EXPECT_CALL(observer_, SyncStartupFailed()).Times(0);
  SyncStartupTracker tracker(profile_.get(), &observer_);
  Mock::VerifyAndClearExpectations(&observer_);
  Mock::VerifyAndClearExpectations(mock_pss_);

  // Now, mark the PSS as having an auth error.
  ON_CALL(*mock_pss_, GetDisableReasons())
      .WillByDefault(Return(syncer::SyncService::DISABLE_REASON_NONE));
  ON_CALL(*mock_pss_, GetTransportState())
      .WillByDefault(Return(syncer::SyncService::TransportState::INITIALIZING));
  GoogleServiceAuthError error(
      GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
  ON_CALL(*mock_pss_, GetAuthError()).WillByDefault(ReturnRef(error));
  EXPECT_CALL(observer_, SyncStartupFailed());
  tracker.OnStateChanged(mock_pss_);
}

TEST_F(SyncStartupTrackerTest, SyncDelayedUnrecoverableError) {
  // Non-initialized PSS should result in no callbacks to the observer.
  SetupNonInitializedPSS();
  EXPECT_CALL(observer_, SyncStartupCompleted()).Times(0);
  EXPECT_CALL(observer_, SyncStartupFailed()).Times(0);
  SyncStartupTracker tracker(profile_.get(), &observer_);
  Mock::VerifyAndClearExpectations(&observer_);
  Mock::VerifyAndClearExpectations(mock_pss_);

  // Now, mark the PSS as having an unrecoverable error.
  ON_CALL(*mock_pss_, GetDisableReasons())
      .WillByDefault(
          Return(syncer::SyncService::DISABLE_REASON_UNRECOVERABLE_ERROR));
  ON_CALL(*mock_pss_, GetTransportState())
      .WillByDefault(Return(syncer::SyncService::TransportState::DISABLED));
  EXPECT_CALL(observer_, SyncStartupFailed());
  tracker.OnStateChanged(mock_pss_);
}

}  // namespace
