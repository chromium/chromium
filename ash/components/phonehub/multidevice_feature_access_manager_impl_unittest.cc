// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/phonehub/multidevice_feature_access_manager_impl.h"

#include <memory>

#include "ash/components/phonehub/fake_connection_scheduler.h"
#include "ash/components/phonehub/fake_feature_status_provider.h"
#include "ash/components/phonehub/fake_message_sender.h"
#include "ash/components/phonehub/notification_access_setup_operation.h"
#include "ash/components/phonehub/pref_names.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace phonehub {
namespace {

using AccessStatus = MultideviceFeatureAccessManager::AccessStatus;
using AccessProhibitedReason =
    MultideviceFeatureAccessManager::AccessProhibitedReason;

class FakeObserver : public MultideviceFeatureAccessManager::Observer {
 public:
  FakeObserver() = default;
  ~FakeObserver() override = default;

  size_t num_calls() const { return num_calls_; }

  // MultideviceFeatureAccessManager::Observer:
  void OnNotificationAccessChanged() override { ++num_calls_; }

  // MultideviceFeatureAccessManager::Observer:
  void OnCameraRollAccessChanged() override { ++num_calls_; }

 private:
  size_t num_calls_ = 0;
};

class FakeOperationDelegate
    : public NotificationAccessSetupOperation::Delegate {
 public:
  FakeOperationDelegate() = default;
  ~FakeOperationDelegate() override = default;

  NotificationAccessSetupOperation::Status status() const { return status_; }

  // NotificationAccessSetupOperation::Delegate:
  void OnStatusChange(
      NotificationAccessSetupOperation::Status new_status) override {
    status_ = new_status;
  }

 private:
  NotificationAccessSetupOperation::Status status_ =
      NotificationAccessSetupOperation::Status::kConnecting;
};

}  // namespace

class MultideviceFeatureAccessManagerImplTest : public testing::Test {
 protected:
  MultideviceFeatureAccessManagerImplTest() = default;
  MultideviceFeatureAccessManagerImplTest(
      const MultideviceFeatureAccessManagerImplTest&) = delete;
  MultideviceFeatureAccessManagerImplTest& operator=(
      const MultideviceFeatureAccessManagerImplTest&) = delete;
  ~MultideviceFeatureAccessManagerImplTest() override = default;

  // testing::Test:
  void SetUp() override {
    MultideviceFeatureAccessManagerImpl::RegisterPrefs(
        pref_service_.registry());
    fake_feature_status_provider_ =
        std::make_unique<FakeFeatureStatusProvider>();
    fake_message_sender_ = std::make_unique<FakeMessageSender>();
    fake_connection_scheduler_ = std::make_unique<FakeConnectionScheduler>();
  }

  void TearDown() override { manager_->RemoveObserver(&fake_observer_); }

  void InitializeAccessStatus(
      AccessStatus notification_expected_status,
      AccessStatus camera_roll_expected_status,
      AccessProhibitedReason reason = AccessProhibitedReason::kUnknown) {
    pref_service_.SetInteger(prefs::kNotificationAccessStatus,
                             static_cast<int>(notification_expected_status));
    pref_service_.SetInteger(prefs::kCameraRollAccessStatus,
                             static_cast<int>(camera_roll_expected_status));
    pref_service_.SetInteger(prefs::kNotificationAccessProhibitedReason,
                             static_cast<int>(reason));
    SetNeedsOneTimeNotificationAccessUpdate(/*needs_update=*/false);
    manager_ = std::make_unique<MultideviceFeatureAccessManagerImpl>(
        &pref_service_, fake_feature_status_provider_.get(),
        fake_message_sender_.get(), fake_connection_scheduler_.get());
    manager_->AddObserver(&fake_observer_);
  }

  NotificationAccessSetupOperation::Status
  GetNotificationAccessSetupOperationStatus() {
    return fake_delegate_.status();
  }

  void VerifyNotificationAccessGrantedState(AccessStatus expected_status) {
    VerifyNotificationAccessGrantedState(expected_status,
                                         AccessProhibitedReason::kUnknown);
  }

  void VerifyNotificationAccessGrantedState(
      AccessStatus expected_status,
      AccessProhibitedReason expected_reason) {
    EXPECT_EQ(static_cast<int>(expected_status),
              pref_service_.GetInteger(prefs::kNotificationAccessStatus));
    EXPECT_EQ(expected_status, manager_->GetNotificationAccessStatus());
    EXPECT_EQ(
        static_cast<int>(expected_reason),
        pref_service_.GetInteger(prefs::kNotificationAccessProhibitedReason));
    EXPECT_EQ(expected_reason,
              manager_->GetNotificationAccessProhibitedReason());
  }

  void VerifyCameraRollAccessGrantedState(AccessStatus expected_status) {
    EXPECT_EQ(static_cast<int>(expected_status),
              pref_service_.GetInteger(prefs::kCameraRollAccessStatus));
    EXPECT_EQ(expected_status, manager_->GetCameraRollAccessStatus());
  }

  bool HasMultideviceFeatureSetupUiBeenDismissed() {
    return manager_->HasMultideviceFeatureSetupUiBeenDismissed();
  }

  void DismissSetupRequiredUi() { manager_->DismissSetupRequiredUi(); }

  std::unique_ptr<NotificationAccessSetupOperation> StartSetupOperation() {
    return manager_->AttemptNotificationSetup(&fake_delegate_);
  }

  bool IsSetupOperationInProgress() {
    return manager_->IsSetupOperationInProgress();
  }

  void SetNotificationAccessStatusInternal(AccessStatus status,
                                           AccessProhibitedReason reason) {
    manager_->SetNotificationAccessStatusInternal(status, reason);
  }

  void SetCameraRollAccessStatusInternal(AccessStatus status) {
    manager_->SetCameraRollAccessStatusInternal(status);
  }

  void SetFeatureStatus(FeatureStatus status) {
    fake_feature_status_provider_->SetStatus(status);
  }

  FeatureStatus GetFeatureStatus() {
    return fake_feature_status_provider_->GetStatus();
  }

  size_t GetNumScheduleConnectionNowCalls() const {
    return fake_connection_scheduler_->num_schedule_connection_now_calls();
  }

  size_t GetNumShowNotificationAccessSetupRequestCount() const {
    return fake_message_sender_->show_notification_access_setup_request_count();
  }

  size_t GetNumObserverCalls() const { return fake_observer_.num_calls(); }

  void SetNeedsOneTimeNotificationAccessUpdate(bool needs_update) {
    pref_service_.SetBoolean(prefs::kNeedsOneTimeNotificationAccessUpdate,
                             needs_update);
  }

 private:
  TestingPrefServiceSimple pref_service_;

  FakeObserver fake_observer_;
  FakeOperationDelegate fake_delegate_;
  std::unique_ptr<FakeFeatureStatusProvider> fake_feature_status_provider_;
  std::unique_ptr<FakeMessageSender> fake_message_sender_;
  std::unique_ptr<FakeConnectionScheduler> fake_connection_scheduler_;
  std::unique_ptr<MultideviceFeatureAccessManager> manager_;
};

TEST_F(MultideviceFeatureAccessManagerImplTest, ShouldShowSetupRequiredUi) {
  // Notification setup is not dismissed initially even when access has been
  // granted.
  InitializeAccessStatus(AccessStatus::kAccessGranted,
                         AccessStatus::kAccessGranted);
  EXPECT_FALSE(HasMultideviceFeatureSetupUiBeenDismissed());

  // Notification setup is not dismissed initially when access has not been
  // granted.
  InitializeAccessStatus(AccessStatus::kAvailableButNotGranted,
                         AccessStatus::kAccessGranted);
  EXPECT_FALSE(HasMultideviceFeatureSetupUiBeenDismissed());

  // Simlulate dismissal of UI.
  DismissSetupRequiredUi();
  EXPECT_TRUE(HasMultideviceFeatureSetupUiBeenDismissed());

  // Dismissal value is persisted on initialization with access not granted.
  InitializeAccessStatus(AccessStatus::kAvailableButNotGranted,
                         AccessStatus::kAccessGranted);
  EXPECT_TRUE(HasMultideviceFeatureSetupUiBeenDismissed());

  // Dismissal value is persisted on initialization with access granted.
  InitializeAccessStatus(AccessStatus::kAccessGranted,
                         AccessStatus::kAccessGranted);
  EXPECT_TRUE(HasMultideviceFeatureSetupUiBeenDismissed());
}

TEST_F(MultideviceFeatureAccessManagerImplTest, AllAccessInitiallyGranted) {
  InitializeAccessStatus(AccessStatus::kAccessGranted,
                         AccessStatus::kAccessGranted);
  VerifyNotificationAccessGrantedState(AccessStatus::kAccessGranted);
  VerifyCameraRollAccessGrantedState(AccessStatus::kAccessGranted);

  // Cannot start the notification access setup flow if notification and camera
  // roll access have already been granted.
  auto operation = StartSetupOperation();
  EXPECT_FALSE(operation);
}

TEST_F(MultideviceFeatureAccessManagerImplTest, OnFeatureStatusChanged) {
  InitializeAccessStatus(AccessStatus::kAvailableButNotGranted,
                         AccessStatus::kAvailableButNotGranted);
  VerifyNotificationAccessGrantedState(AccessStatus::kAvailableButNotGranted);
  VerifyCameraRollAccessGrantedState(AccessStatus::kAvailableButNotGranted);

  // Set initial state to disconnected.
  SetFeatureStatus(FeatureStatus::kEnabledButDisconnected);
  EXPECT_EQ(0u, GetNumShowNotificationAccessSetupRequestCount());
  EXPECT_EQ(NotificationAccessSetupOperation::Status::kConnecting,
            GetNotificationAccessSetupOperationStatus());
  // Simulate feature status to be enabled and connected. SetupOperation is
  // also not in progress, so expect no new requests to be sent.
  SetFeatureStatus(FeatureStatus::kEnabledAndConnected);
  EXPECT_EQ(0u, GetNumShowNotificationAccessSetupRequestCount());
  EXPECT_EQ(NotificationAccessSetupOperation::Status::kConnecting,
            GetNotificationAccessSetupOperationStatus());
  // Simulate setup operation is in progress. This will trigger a sent
  // request.
  auto operation = StartSetupOperation();
  EXPECT_TRUE(operation);
  EXPECT_EQ(1u, GetNumShowNotificationAccessSetupRequestCount());
  EXPECT_EQ(NotificationAccessSetupOperation::Status::
                kSentMessageToPhoneAndWaitingForResponse,
            GetNotificationAccessSetupOperationStatus());

  // Set another feature status, expect status to be updated.
  SetFeatureStatus(FeatureStatus::kEnabledButDisconnected);
  EXPECT_EQ(1u, GetNumShowNotificationAccessSetupRequestCount());
  EXPECT_EQ(NotificationAccessSetupOperation::Status::kConnectionDisconnected,
            GetNotificationAccessSetupOperationStatus());
}

TEST_F(MultideviceFeatureAccessManagerImplTest, StartDisconnectedAndNoAccess) {
  // Set initial state to disconnected.
  SetFeatureStatus(FeatureStatus::kEnabledButDisconnected);

  InitializeAccessStatus(AccessStatus::kAvailableButNotGranted,
                         AccessStatus::kAccessGranted);
  VerifyNotificationAccessGrantedState(AccessStatus::kAvailableButNotGranted);
  VerifyCameraRollAccessGrantedState(AccessStatus::kAccessGranted);

  // Start a setup operation with enabled but disconnected status and access
  // not granted.
  auto operation = StartSetupOperation();
  EXPECT_TRUE(operation);
  EXPECT_EQ(1u, GetNumScheduleConnectionNowCalls());

  // Simulate changing states from connecting to connected.
  SetFeatureStatus(FeatureStatus::kEnabledAndConnecting);
  SetFeatureStatus(FeatureStatus::kEnabledAndConnected);

  // Verify that the request message has been sent and our operation status
  // is updated.
  EXPECT_EQ(1u, GetNumShowNotificationAccessSetupRequestCount());
  EXPECT_EQ(NotificationAccessSetupOperation::Status::
                kSentMessageToPhoneAndWaitingForResponse,
            GetNotificationAccessSetupOperationStatus());

  // Simulate getting a response back from the phone.
  SetNotificationAccessStatusInternal(AccessStatus::kAccessGranted,
                                      AccessProhibitedReason::kUnknown);
  VerifyNotificationAccessGrantedState(AccessStatus::kAccessGranted);
  EXPECT_EQ(NotificationAccessSetupOperation::Status::kCompletedSuccessfully,
            GetNotificationAccessSetupOperationStatus());
}

TEST_F(MultideviceFeatureAccessManagerImplTest,
       StartDisconnectedAndNoAccess_NotificationAccessIsProhibited) {
  // Set initial state to disconnected.
  SetFeatureStatus(FeatureStatus::kEnabledButDisconnected);

  InitializeAccessStatus(AccessStatus::kAvailableButNotGranted,
                         AccessStatus::kAvailableButNotGranted);
  VerifyNotificationAccessGrantedState(AccessStatus::kAvailableButNotGranted);
  VerifyCameraRollAccessGrantedState(AccessStatus::kAvailableButNotGranted);

  // Start a setup operation with enabled but disconnected status and access
  // not granted.
  auto operation = StartSetupOperation();
  EXPECT_TRUE(operation);
  EXPECT_EQ(1u, GetNumScheduleConnectionNowCalls());

  // Simulate changing states from connecting to connected.
  SetFeatureStatus(FeatureStatus::kEnabledAndConnecting);
  SetFeatureStatus(FeatureStatus::kEnabledAndConnected);

  // Verify that the request message has been sent and our operation status
  // is updated.
  EXPECT_EQ(1u, GetNumShowNotificationAccessSetupRequestCount());
  EXPECT_EQ(NotificationAccessSetupOperation::Status::
                kSentMessageToPhoneAndWaitingForResponse,
            GetNotificationAccessSetupOperationStatus());

  // Simulate getting a response back from the phone.
  SetNotificationAccessStatusInternal(
      AccessStatus::kProhibited,
      AccessProhibitedReason::kDisabledByPhonePolicy);
  VerifyNotificationAccessGrantedState(
      AccessStatus::kProhibited,
      AccessProhibitedReason::kDisabledByPhonePolicy);
  EXPECT_EQ(
      NotificationAccessSetupOperation::Status::kProhibitedFromProvidingAccess,
      GetNotificationAccessSetupOperationStatus());
}

TEST_F(MultideviceFeatureAccessManagerImplTest, StartConnectingAndNoAccess) {
  // Set initial state to connecting.
  SetFeatureStatus(FeatureStatus::kEnabledAndConnecting);

  InitializeAccessStatus(AccessStatus::kAvailableButNotGranted,
                         AccessStatus::kAvailableButNotGranted);
  VerifyNotificationAccessGrantedState(AccessStatus::kAvailableButNotGranted);
  VerifyCameraRollAccessGrantedState(AccessStatus::kAvailableButNotGranted);

  // Start a setup operation with enabled and connecting status and access
  // not granted.
  auto operation = StartSetupOperation();
  EXPECT_TRUE(operation);

  // Simulate changing states from connecting to connected.
  SetFeatureStatus(FeatureStatus::kEnabledAndConnected);

  // Verify that the request message has been sent and our operation status
  // is updated.
  EXPECT_EQ(1u, GetNumShowNotificationAccessSetupRequestCount());
  EXPECT_EQ(NotificationAccessSetupOperation::Status::
                kSentMessageToPhoneAndWaitingForResponse,
            GetNotificationAccessSetupOperationStatus());

  // Simulate getting a response back from the phone.
  SetNotificationAccessStatusInternal(AccessStatus::kAccessGranted,
                                      AccessProhibitedReason::kUnknown);
  VerifyNotificationAccessGrantedState(AccessStatus::kAccessGranted);
  EXPECT_EQ(NotificationAccessSetupOperation::Status::kCompletedSuccessfully,
            GetNotificationAccessSetupOperationStatus());
}

TEST_F(MultideviceFeatureAccessManagerImplTest, StartConnectedAndNoAccess) {
  // Set initial state to connected.
  SetFeatureStatus(FeatureStatus::kEnabledAndConnected);

  InitializeAccessStatus(AccessStatus::kAvailableButNotGranted,
                         AccessStatus::kAvailableButNotGranted);
  VerifyNotificationAccessGrantedState(AccessStatus::kAvailableButNotGranted);
  VerifyCameraRollAccessGrantedState(AccessStatus::kAvailableButNotGranted);

  // Start a setup operation with enabled and connected status and access
  // not granted.
  auto operation = StartSetupOperation();
  EXPECT_TRUE(operation);

  // Verify that the request message has been sent and our operation status
  // is updated.
  EXPECT_EQ(1u, GetNumShowNotificationAccessSetupRequestCount());
  EXPECT_EQ(NotificationAccessSetupOperation::Status::
                kSentMessageToPhoneAndWaitingForResponse,
            GetNotificationAccessSetupOperationStatus());

  // Simulate getting a response back from the phone.
  SetNotificationAccessStatusInternal(AccessStatus::kAccessGranted,
                                      AccessProhibitedReason::kUnknown);
  VerifyNotificationAccessGrantedState(AccessStatus::kAccessGranted);
  EXPECT_EQ(NotificationAccessSetupOperation::Status::kCompletedSuccessfully,
            GetNotificationAccessSetupOperationStatus());
}

TEST_F(MultideviceFeatureAccessManagerImplTest,
       SimulateConnectingToDisconnected) {
  // Set initial state to connecting.
  SetFeatureStatus(FeatureStatus::kEnabledAndConnecting);

  InitializeAccessStatus(AccessStatus::kAvailableButNotGranted,
                         AccessStatus::kAvailableButNotGranted);
  VerifyNotificationAccessGrantedState(AccessStatus::kAvailableButNotGranted);
  VerifyCameraRollAccessGrantedState(AccessStatus::kAvailableButNotGranted);

  auto operation = StartSetupOperation();
  EXPECT_TRUE(operation);

  // Simulate a disconnection and expect that status has been updated.
  SetFeatureStatus(FeatureStatus::kEnabledButDisconnected);
  EXPECT_EQ(NotificationAccessSetupOperation::Status::kTimedOutConnecting,
            GetNotificationAccessSetupOperationStatus());
}

TEST_F(MultideviceFeatureAccessManagerImplTest,
       SimulateConnectedToDisconnected) {
  // Simulate connected state.
  SetFeatureStatus(FeatureStatus::kEnabledAndConnected);

  InitializeAccessStatus(AccessStatus::kAvailableButNotGranted,
                         AccessStatus::kAvailableButNotGranted);
  VerifyNotificationAccessGrantedState(AccessStatus::kAvailableButNotGranted);
  VerifyCameraRollAccessGrantedState(AccessStatus::kAvailableButNotGranted);

  auto operation = StartSetupOperation();
  EXPECT_TRUE(operation);

  EXPECT_EQ(1u, GetNumShowNotificationAccessSetupRequestCount());

  // Simulate a disconnection, expect status update.
  SetFeatureStatus(FeatureStatus::kEnabledButDisconnected);
  EXPECT_EQ(NotificationAccessSetupOperation::Status::kConnectionDisconnected,
            GetNotificationAccessSetupOperationStatus());
}

TEST_F(MultideviceFeatureAccessManagerImplTest, SimulateConnectedToDisabled) {
  // Simulate connected state.
  SetFeatureStatus(FeatureStatus::kEnabledAndConnected);

  InitializeAccessStatus(AccessStatus::kAvailableButNotGranted,
                         AccessStatus::kAvailableButNotGranted);
  VerifyNotificationAccessGrantedState(AccessStatus::kAvailableButNotGranted);
  VerifyCameraRollAccessGrantedState(AccessStatus::kAvailableButNotGranted);

  auto operation = StartSetupOperation();
  EXPECT_TRUE(operation);

  EXPECT_EQ(1u, GetNumShowNotificationAccessSetupRequestCount());

  // Simulate disabling the feature, expect status update.
  SetFeatureStatus(FeatureStatus::kDisabled);
  EXPECT_EQ(NotificationAccessSetupOperation::Status::kConnectionDisconnected,
            GetNotificationAccessSetupOperationStatus());
}

TEST_F(MultideviceFeatureAccessManagerImplTest,
       FlipNotificationAccessGrantedToNotGranted) {
  InitializeAccessStatus(AccessStatus::kAccessGranted,
                         AccessStatus::kAccessGranted);
  VerifyNotificationAccessGrantedState(AccessStatus::kAccessGranted);
  VerifyCameraRollAccessGrantedState(AccessStatus::kAccessGranted);

  // Simulate flipping notification access state to no granted.
  SetNotificationAccessStatusInternal(AccessStatus::kAvailableButNotGranted,
                                      AccessProhibitedReason::kUnknown);
  VerifyNotificationAccessGrantedState(AccessStatus::kAvailableButNotGranted);
  EXPECT_EQ(1u, GetNumObserverCalls());
}

TEST_F(MultideviceFeatureAccessManagerImplTest,
       FlipNotificationAccessGrantedToProhibited) {
  InitializeAccessStatus(AccessStatus::kAccessGranted,
                         AccessStatus::kAccessGranted);
  VerifyNotificationAccessGrantedState(AccessStatus::kAccessGranted);
  VerifyCameraRollAccessGrantedState(AccessStatus::kAccessGranted);

  // Simulate flipping notification access state to prohibited.
  SetNotificationAccessStatusInternal(
      AccessStatus::kProhibited,
      AccessProhibitedReason::kDisabledByPhonePolicy);
  VerifyNotificationAccessGrantedState(
      AccessStatus::kProhibited,
      AccessProhibitedReason::kDisabledByPhonePolicy);
  EXPECT_EQ(1u, GetNumObserverCalls());
}

TEST_F(MultideviceFeatureAccessManagerImplTest,
       FlipCameraRollAccessGrantedToNotGranted) {
  InitializeAccessStatus(AccessStatus::kAccessGranted,
                         AccessStatus::kAccessGranted);
  VerifyNotificationAccessGrantedState(AccessStatus::kAccessGranted);
  VerifyCameraRollAccessGrantedState(AccessStatus::kAccessGranted);

  // Simulate flipping camera roll access state to no granted.
  SetCameraRollAccessStatusInternal(AccessStatus::kAvailableButNotGranted);
  VerifyCameraRollAccessGrantedState(AccessStatus::kAvailableButNotGranted);
  EXPECT_EQ(1u, GetNumObserverCalls());
}

TEST_F(MultideviceFeatureAccessManagerImplTest, AccessNotChanged) {
  InitializeAccessStatus(AccessStatus::kAccessGranted,
                         AccessStatus::kAccessGranted);
  VerifyNotificationAccessGrantedState(AccessStatus::kAccessGranted);
  VerifyCameraRollAccessGrantedState(AccessStatus::kAccessGranted);

  // If the access state is unchanged, we do not expect any notifications.
  SetNotificationAccessStatusInternal(AccessStatus::kAccessGranted,
                                      AccessProhibitedReason::kUnknown);
  VerifyNotificationAccessGrantedState(AccessStatus::kAccessGranted);
  EXPECT_EQ(0u, GetNumObserverCalls());
}

TEST_F(MultideviceFeatureAccessManagerImplTest,
       NeedsOneTimeNotificationAccessUpdate_AccessGranted) {
  InitializeAccessStatus(AccessStatus::kAccessGranted,
                         AccessStatus::kAccessGranted);
  VerifyNotificationAccessGrantedState(AccessStatus::kAccessGranted);
  VerifyCameraRollAccessGrantedState(AccessStatus::kAccessGranted);

  // Send a one-time signal to observers if access is granted. See
  // http://crbug.com/1215559.
  SetNeedsOneTimeNotificationAccessUpdate(/*needs_update=*/true);
  SetNotificationAccessStatusInternal(AccessStatus::kAccessGranted,
                                      AccessProhibitedReason::kUnknown);
  VerifyNotificationAccessGrantedState(AccessStatus::kAccessGranted);
  EXPECT_EQ(1u, GetNumObserverCalls());

  // Observers should be notified only once ever.
  SetNotificationAccessStatusInternal(AccessStatus::kAccessGranted,
                                      AccessProhibitedReason::kUnknown);
  VerifyNotificationAccessGrantedState(AccessStatus::kAccessGranted);
  EXPECT_EQ(1u, GetNumObserverCalls());
}

TEST_F(MultideviceFeatureAccessManagerImplTest,
       NeedsOneTimeNotificationAccessUpdate_Prohibited) {
  InitializeAccessStatus(AccessStatus::kProhibited,
                         AccessStatus::kAccessGranted,
                         AccessProhibitedReason::kDisabledByPhonePolicy);
  VerifyNotificationAccessGrantedState(
      AccessStatus::kProhibited,
      AccessProhibitedReason::kDisabledByPhonePolicy);
  VerifyCameraRollAccessGrantedState(AccessStatus::kAccessGranted);

  // Only send the one-time signal to observers if access is granted. See
  // http://crbug.com/1215559.
  SetNeedsOneTimeNotificationAccessUpdate(/*needs_update=*/true);
  SetNotificationAccessStatusInternal(
      AccessStatus::kProhibited,
      AccessProhibitedReason::kDisabledByPhonePolicy);
  VerifyNotificationAccessGrantedState(
      AccessStatus::kProhibited,
      AccessProhibitedReason::kDisabledByPhonePolicy);
  EXPECT_EQ(0u, GetNumObserverCalls());
}

TEST_F(MultideviceFeatureAccessManagerImplTest,
       NotificationAccessProhibitedReason_FromProhibited) {
  InitializeAccessStatus(AccessStatus::kProhibited,
                         AccessStatus::kAccessGranted);
  VerifyNotificationAccessGrantedState(AccessStatus::kProhibited);

  // Simulates an initial update after the pref is first added
  SetNotificationAccessStatusInternal(AccessStatus::kProhibited,
                                      AccessProhibitedReason::kWorkProfile);
  VerifyNotificationAccessGrantedState(AccessStatus::kProhibited,
                                       AccessProhibitedReason::kWorkProfile);
  EXPECT_EQ(1u, GetNumObserverCalls());

  // No update or observer notification should occur with no change
  SetNotificationAccessStatusInternal(AccessStatus::kProhibited,
                                      AccessProhibitedReason::kWorkProfile);
  VerifyNotificationAccessGrantedState(AccessStatus::kProhibited,
                                       AccessProhibitedReason::kWorkProfile);
  EXPECT_EQ(1u, GetNumObserverCalls());

  // This can happen if a user updates from Android <N to >=N
  SetNotificationAccessStatusInternal(
      AccessStatus::kProhibited,
      AccessProhibitedReason::kDisabledByPhonePolicy);
  VerifyNotificationAccessGrantedState(
      AccessStatus::kProhibited,
      AccessProhibitedReason::kDisabledByPhonePolicy);
  EXPECT_EQ(2u, GetNumObserverCalls());
}

TEST_F(MultideviceFeatureAccessManagerImplTest,
       NotificationAccessProhibitedReason_FromGranted) {
  InitializeAccessStatus(AccessStatus::kAccessGranted,
                         AccessStatus::kAccessGranted);
  VerifyNotificationAccessGrantedState(AccessStatus::kAccessGranted);

  SetNotificationAccessStatusInternal(
      AccessStatus::kProhibited,
      AccessProhibitedReason::kDisabledByPhonePolicy);
  VerifyNotificationAccessGrantedState(
      AccessStatus::kProhibited,
      AccessProhibitedReason::kDisabledByPhonePolicy);
  EXPECT_EQ(1u, GetNumObserverCalls());
}

}  // namespace phonehub
}  // namespace ash
