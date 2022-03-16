// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_sync_controller_delegate_android.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "chrome/browser/password_manager/android/android_backend_error.h"
#include "chrome/browser/password_manager/android/mock_password_sync_controller_delegate_bridge.h"
#include "components/password_manager/core/browser/mock_password_store_backend.h"
#include "components/sync/engine/data_type_activation_response.h"
#include "components/sync/model/data_type_activation_request.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

namespace {

using ::testing::Return;
using ::testing::StrictMock;

}  // namespace

class PasswordSyncControllerDelegateAndroidTest : public testing::Test {
 protected:
  PasswordSyncControllerDelegateAndroidTest() {
    sync_controller_delegate_ =
        std::make_unique<PasswordSyncControllerDelegateAndroid>(
            CreateBridge(), &sync_delegate_);
  }

  ~PasswordSyncControllerDelegateAndroidTest() override {
    testing::Mock::VerifyAndClearExpectations(bridge_);
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  MockPasswordSyncControllerDelegateBridge* bridge() { return bridge_; }
  MockPasswordBackendSyncDelegate* sync_delegate() { return &sync_delegate_; }
  PasswordSyncControllerDelegateAndroid* sync_controller_delegate() {
    return sync_controller_delegate_.get();
  }
  PasswordSyncControllerDelegateBridge::Consumer& consumer() {
    return *sync_controller_delegate_;
  }

 private:
  std::unique_ptr<PasswordSyncControllerDelegateBridge> CreateBridge() {
    auto unique_delegate_bridge = std::make_unique<
        StrictMock<MockPasswordSyncControllerDelegateBridge>>();
    bridge_ = unique_delegate_bridge.get();
    EXPECT_CALL(*bridge_, SetConsumer);
    return unique_delegate_bridge;
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  MockPasswordBackendSyncDelegate sync_delegate_;
  std::unique_ptr<PasswordSyncControllerDelegateAndroid>
      sync_controller_delegate_;
  raw_ptr<StrictMock<MockPasswordSyncControllerDelegateBridge>> bridge_;
};

TEST_F(PasswordSyncControllerDelegateAndroidTest,
       UpdateSyncStatusOnStartUpSyncDisabled) {
  // We don't care about returned value, as only calls to sync_delegate() are
  // interesting.
  sync_controller_delegate()->CreateProxyModelControllerDelegate();

  EXPECT_CALL(*sync_delegate(), IsSyncingPasswordsEnabled)
      .WillOnce(Return(false));

  RunUntilIdle();
}

TEST_F(PasswordSyncControllerDelegateAndroidTest,
       UpdateSyncStatusOnStartUpSyncEnabled) {
  // We don't care about returned value, as only calls to sync_delegate() are
  // interesting.
  sync_controller_delegate()->CreateProxyModelControllerDelegate();

  EXPECT_CALL(*sync_delegate(), IsSyncingPasswordsEnabled)
      .WillOnce(Return(true));
  EXPECT_CALL(*sync_delegate(), GetSyncingAccount).Times(1);

  RunUntilIdle();
}

TEST_F(PasswordSyncControllerDelegateAndroidTest, OnSyncStarting) {
  syncer::DataTypeActivationRequest test_request;

  EXPECT_CALL(*bridge(), NotifyCredentialManagerWhenSyncing);
  sync_controller_delegate()->OnSyncStarting(test_request, base::DoNothing());

  RunUntilIdle();
}

TEST_F(PasswordSyncControllerDelegateAndroidTest, OnSyncStoppingTemporary) {
  EXPECT_CALL(*bridge(), NotifyCredentialManagerWhenNotSyncing).Times(0);
  sync_controller_delegate()->OnSyncStopping(syncer::KEEP_METADATA);

  RunUntilIdle();
}

TEST_F(PasswordSyncControllerDelegateAndroidTest, OnSyncStoppingPermanently) {
  EXPECT_CALL(*bridge(), NotifyCredentialManagerWhenNotSyncing);
  sync_controller_delegate()->OnSyncStopping(syncer::CLEAR_METADATA);

  RunUntilIdle();
}

TEST_F(PasswordSyncControllerDelegateAndroidTest,
       MetrcisWhenCredentialManagerNotificationSucceeds) {
  base::HistogramTester histogram_tester;

  // Imitate credential manager notification success and check recorded metrics.
  consumer().OnCredentialManagerNotified();
  histogram_tester.ExpectBucketCount(
      "PasswordManager.SyncControllerDelegateNotifiesCredentialManager.Success",
      true, 1);
  histogram_tester.ExpectTotalCount(
      "PasswordManager.SyncControllerDelegateNotifiesCredentialManager.Success",
      1);
}

TEST_F(PasswordSyncControllerDelegateAndroidTest,
       MetrcisWhenCredentialManagerNotificationFails) {
  base::HistogramTester histogram_tester;

  // Imitate failure and check recorded metrics.
  AndroidBackendError expected_error(AndroidBackendErrorType::kUncategorized);
  consumer().OnCredentialManagerError(expected_error, 0);

  histogram_tester.ExpectBucketCount(
      "PasswordManager.SyncControllerDelegateNotifiesCredentialManager.Success",
      false, 1);
  histogram_tester.ExpectTotalCount(
      "PasswordManager.SyncControllerDelegateNotifiesCredentialManager.Success",
      1);
  histogram_tester.ExpectBucketCount(
      "PasswordManager.SyncControllerDelegateNotifiesCredentialManager."
      "ErrorCode",
      expected_error.type, 1);
  histogram_tester.ExpectTotalCount(
      "PasswordManager.SyncControllerDelegateNotifiesCredentialManager."
      "ErrorCode",
      1);
}

}  // namespace password_manager
