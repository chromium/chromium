// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_sync_controller_delegate_android.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "chrome/browser/password_manager/android/mock_password_sync_controller_delegate_bridge.h"
#include "components/password_manager/core/browser/android_backend_error.h"
#include "components/password_manager/core/browser/mock_password_store_backend.h"
#include "components/sync/engine/data_type_activation_response.h"
#include "components/sync/model/data_type_activation_request.h"
#include "components/sync/test/test_sync_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

namespace {

using base::Bucket;
using ::testing::ElementsAre;
using ::testing::Return;
using ::testing::StrictMock;

}  // namespace

class PasswordSyncControllerDelegateAndroidTest : public testing::Test {
 protected:
  PasswordSyncControllerDelegateAndroidTest() {
    sync_controller_delegate_ =
        std::make_unique<PasswordSyncControllerDelegateAndroid>(
            CreateBridge(), base::DoNothing());
  }

  ~PasswordSyncControllerDelegateAndroidTest() override {
    testing::Mock::VerifyAndClearExpectations(bridge_);
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  MockPasswordSyncControllerDelegateBridge* bridge() { return bridge_; }
  syncer::SyncService* sync_service() { return &sync_service_; }
  PasswordSyncControllerDelegateAndroid* sync_controller_delegate() {
    return sync_controller_delegate_.get();
  }
  PasswordSyncControllerDelegateBridge::Consumer& consumer() {
    return *sync_controller_delegate_;
  }

  std::unique_ptr<PasswordSyncControllerDelegateBridge> CreateBridge() {
    auto unique_delegate_bridge = std::make_unique<
        StrictMock<MockPasswordSyncControllerDelegateBridge>>();
    bridge_ = unique_delegate_bridge.get();
    EXPECT_CALL(*bridge_, SetConsumer);
    return unique_delegate_bridge;
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  syncer::TestSyncService sync_service_;
  std::unique_ptr<PasswordSyncControllerDelegateAndroid>
      sync_controller_delegate_;
  raw_ptr<StrictMock<MockPasswordSyncControllerDelegateBridge>> bridge_;
};

TEST_F(PasswordSyncControllerDelegateAndroidTest,
       OnSyncStatusChangedToEnabledAfterStartup) {
  syncer::TestSyncService sync_service;

  EXPECT_CALL(*bridge(), NotifyCredentialManagerWhenSyncing);
  sync_controller_delegate()->OnStateChanged(&sync_service);

  // Check that observing the same event again will not trigger another
  // notification.
  EXPECT_CALL(*bridge(), NotifyCredentialManagerWhenSyncing).Times(0);
  sync_controller_delegate()->OnStateChanged(&sync_service);
}

TEST_F(PasswordSyncControllerDelegateAndroidTest,
       OnSyncStatusChangedToEnabledExcludingPasswords) {
  syncer::TestSyncService sync_service;
  sync_service.GetUserSettings()->SetSelectedTypes(/*sync_everything=*/false,
                                                   /*types=*/{});

  EXPECT_CALL(*bridge(), NotifyCredentialManagerWhenSyncing).Times(0);
  EXPECT_CALL(*bridge(), NotifyCredentialManagerWhenNotSyncing);
  sync_controller_delegate()->OnStateChanged(&sync_service);
}

TEST_F(PasswordSyncControllerDelegateAndroidTest,
       OnSyncStatusChangedToEnabledFromDisabled) {
  syncer::TestSyncService sync_service;
  sync_service.SetDisableReasons(
      syncer::SyncService::DISABLE_REASON_NOT_SIGNED_IN);

  EXPECT_CALL(*bridge(), NotifyCredentialManagerWhenNotSyncing);
  sync_controller_delegate()->OnStateChanged(&sync_service);

  // Set empty disable reasons set to imitate sync enabling.
  sync_service.SetDisableReasons({});

  EXPECT_CALL(*bridge(), NotifyCredentialManagerWhenSyncing);
  sync_controller_delegate()->OnStateChanged(&sync_service);
}

TEST_F(PasswordSyncControllerDelegateAndroidTest,
       OnSyncStatusChangedToDisabled) {
  syncer::TestSyncService sync_service;
  sync_service.SetDisableReasons(
      syncer::SyncService::DISABLE_REASON_NOT_SIGNED_IN);

  EXPECT_CALL(*bridge(), NotifyCredentialManagerWhenNotSyncing);
  sync_controller_delegate()->OnStateChanged(&sync_service);

  // Check that observing the same event again will not trigger another
  // notification.
  EXPECT_CALL(*bridge(), NotifyCredentialManagerWhenNotSyncing).Times(0);
  sync_controller_delegate()->OnStateChanged(&sync_service);
}

TEST_F(PasswordSyncControllerDelegateAndroidTest,
       OnSyncStatusChangedToDisabledFromEnabled) {
  syncer::TestSyncService sync_service;

  EXPECT_CALL(*bridge(), NotifyCredentialManagerWhenSyncing);
  sync_controller_delegate()->OnStateChanged(&sync_service);

  // Set disable reasons to imitate sync disabling.
  sync_service.SetDisableReasons(
      syncer::SyncService::DISABLE_REASON_NOT_SIGNED_IN);

  EXPECT_CALL(*bridge(), NotifyCredentialManagerWhenNotSyncing);
  sync_controller_delegate()->OnStateChanged(&sync_service);
}

TEST_F(PasswordSyncControllerDelegateAndroidTest,
       MetrcisWhenCredentialManagerNotificationSucceeds) {
  base::HistogramTester histogram_tester;

  // Imitate credential manager notification success and check recorded metrics.
  consumer().OnCredentialManagerNotified();
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "PasswordManager.SyncControllerDelegateNotifiesCredentialManager."
          "Success"),
      ElementsAre(Bucket(true, 1)));
}

TEST_F(PasswordSyncControllerDelegateAndroidTest,
       MetrcisWhenCredentialManagerNotificationFails) {
  base::HistogramTester histogram_tester;

  // Imitate failure and check recorded metrics.
  AndroidBackendError expected_error(AndroidBackendErrorType::kUncategorized);
  consumer().OnCredentialManagerError(expected_error, 0);

  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "PasswordManager.SyncControllerDelegateNotifiesCredentialManager."
          "Success"),
      ElementsAre(Bucket(false, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "PasswordManager.SyncControllerDelegateNotifiesCredentialManager."
          "ErrorCode"),
      ElementsAre(Bucket(expected_error.type, 1)));
  histogram_tester.ExpectTotalCount(
      "PasswordManager.SyncControllerDelegateNotifiesCredentialManager."
      "APIErrorCode",
      0);
}

TEST_F(PasswordSyncControllerDelegateAndroidTest,
       MetrcisWhenCredentialManagerNotificationFailsAPIError) {
  base::HistogramTester histogram_tester;

  // Imitate failure and check recorded metrics.
  AndroidBackendError expected_error(AndroidBackendErrorType::kExternalError);
  constexpr int expected_api_error_code = 43507;
  consumer().OnCredentialManagerError(expected_error, expected_api_error_code);

  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "PasswordManager.SyncControllerDelegateNotifiesCredentialManager."
          "Success"),
      ElementsAre(Bucket(false, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "PasswordManager.SyncControllerDelegateNotifiesCredentialManager."
          "ErrorCode"),
      ElementsAre(Bucket(expected_error.type, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "PasswordManager.SyncControllerDelegateNotifiesCredentialManager."
          "APIErrorCode"),
      ElementsAre(Bucket(expected_api_error_code, 1)));
}

TEST_F(PasswordSyncControllerDelegateAndroidTest,
       AttachesObserverOnSyncServiceInitialized) {
  sync_controller_delegate()->OnSyncServiceInitialized(sync_service());
  EXPECT_TRUE(sync_service()->HasObserver(sync_controller_delegate()));
}

TEST_F(PasswordSyncControllerDelegateAndroidTest, OnSyncShutdown) {
  base::MockCallback<base::OnceClosure> mock_callback;
  auto sync_controller =
      std::make_unique<PasswordSyncControllerDelegateAndroid>(
          CreateBridge(), mock_callback.Get());
  syncer::TestSyncService sync_service;

  EXPECT_CALL(mock_callback, Run);
  sync_controller->OnSyncShutdown(&sync_service);
}

}  // namespace password_manager
