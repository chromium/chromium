// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_sync_controller_delegate_android.h"

#include <memory>

#include "base/test/task_environment.h"
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

 private:
  std::unique_ptr<PasswordSyncControllerDelegateBridge> CreateBridge() {
    auto unique_delegate_bridge = std::make_unique<
        StrictMock<MockPasswordSyncControllerDelegateBridge>>();
    bridge_ = unique_delegate_bridge.get();
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

}  // namespace password_manager
